#!/usr/bin/env node
import {
  AgentWorkflowError,
  agentWorkflowTools,
  createAgentWorkflow,
} from '../src/agent-workflow.js';
import { createAgentWorkflowFiles } from './agent-workflow-files.js';

const usage =
  'node desktop/cli/agent-workflow.js --workspace ABSOLUTE_PATH [--chromium-path ABSOLUTE_PATH]';
const maxRequest = 16 * 1024;
const protocolVersion = '2025-06-18';
const isObject = (value) => value !== null && typeof value === 'object' && !Array.isArray(value);
const rpcError = (id, code, message) => ({ jsonrpc: '2.0', id, error: { code, message } });
const rpcResult = (id, result) => ({ jsonrpc: '2.0', id, result });

function options(args) {
  const values = new Map();
  for (let index = 0; index < args.length; index += 2) {
    const flag = args[index],
      value = args[index + 1];
    if (
      !['--workspace', '--chromium-path'].includes(flag) ||
      values.has(flag) ||
      !value ||
      value.startsWith('--')
    )
      throw new AgentWorkflowError('USAGE', usage);
    values.set(flag, value);
  }
  if (!values.has('--workspace')) throw new AgentWorkflowError('USAGE', usage);
  return { workspace: values.get('--workspace'), chromiumPath: values.get('--chromium-path') };
}

// MCP stdio uses one UTF-8 JSON-RPC message per line. Limit bytes *before*
// decoding/JSON.parse, and resynchronize on the next newline after a bad frame.
async function* frames(input) {
  let size = 0,
    tooLarge = false,
    parts = [];
  for await (const chunk of input) {
    let start = 0;
    while (start < chunk.length) {
      const newline = chunk.indexOf(10, start),
        end = newline === -1 ? chunk.length : newline;
      if (!tooLarge) {
        size += end - start;
        tooLarge = size > maxRequest;
        if (tooLarge) parts = [];
        else if (start !== end) parts.push(chunk.subarray(start, end));
      }
      if (newline === -1) break;
      yield tooLarge ? null : Buffer.concat(parts, size);
      size = 0;
      tooLarge = false;
      parts = [];
      start = newline + 1;
    }
  }
  if (tooLarge || size) yield null;
}

function mcp(workflow) {
  let phase = 'launch';
  return async (bytes) => {
    if (!bytes) return rpcError(null, -32700, 'Frame exceeds 16 KiB or is missing its newline.');
    let message;
    try {
      message = JSON.parse(new TextDecoder('utf-8', { fatal: true }).decode(bytes));
    } catch {
      return rpcError(null, -32700, 'Expected one UTF-8 JSON-RPC message.');
    }
    const hasId = isObject(message) && Object.hasOwn(message, 'id');
    const id = message?.id;
    const validId = (typeof id === 'string' && id.length <= 200) || Number.isSafeInteger(id);
    if (
      !isObject(message) ||
      message.jsonrpc !== '2.0' ||
      typeof message.method !== 'string' ||
      (hasId && !validId) ||
      Object.hasOwn(message, 'result') ||
      Object.hasOwn(message, 'error')
    )
      return rpcError(validId ? id : null, -32600, 'Invalid JSON-RPC request.');
    const { method, params } = message;
    if (!hasId) {
      if (
        method === 'notifications/initialized' &&
        phase === 'initializing' &&
        (params === undefined || isObject(params))
      )
        phase = 'ready';
      return;
    }
    if (params !== undefined && !isObject(params)) return rpcError(id, -32602, 'Invalid params.');
    if (method === 'ping') return rpcResult(id, {});
    if (method === 'initialize') {
      if (phase !== 'launch') return rpcError(id, -32600, 'Already initialized.');
      if (
        typeof params?.protocolVersion !== 'string' ||
        !isObject(params?.capabilities) ||
        typeof params.clientInfo?.name !== 'string' ||
        typeof params.clientInfo?.version !== 'string'
      )
        return rpcError(
          id,
          -32602,
          'Initialize requires protocolVersion, capabilities and clientInfo.',
        );
      phase = 'initializing';
      return rpcResult(id, {
        protocolVersion,
        serverInfo: { name: 'nect-agent-workflow', version: '0.1.0' },
        capabilities: { tools: {} },
      });
    }
    if (phase !== 'ready')
      return rpcError(id, -32002, 'Initialize and send notifications/initialized first.');
    if (method === 'tools/list') {
      if (params?.cursor !== undefined)
        return rpcError(id, -32602, 'No pagination cursor was issued.');
      return rpcResult(id, { tools: agentWorkflowTools() });
    }
    if (method === 'tools/call') {
      if (
        typeof params?.name !== 'string' ||
        (params.arguments !== undefined && !isObject(params.arguments))
      )
        return rpcError(id, -32602, 'tools/call requires name and optional object arguments.');
      const reply = await workflow.dispatch(params.name, params.arguments);
      if (!reply.ok && reply.error.code === 'UNKNOWN_OPERATION')
        return rpcError(id, -32602, reply.error.message);
      return rpcResult(id, {
        content: [{ type: 'text', text: JSON.stringify(reply) }],
        isError: !reply.ok,
      });
    }
    return rpcError(id, -32601, 'Method not found.');
  };
}

try {
  if (process.argv.length === 3 && process.argv[2] === '--help') {
    console.log(usage);
  } else {
    const { workspace, chromiumPath } = options(process.argv.slice(2));
    const files = await createAgentWorkflowFiles(workspace, { chromiumPath });
    const handle = mcp(createAgentWorkflow({ files }));
    for await (const bytes of frames(process.stdin)) {
      const reply = await handle(bytes);
      if (reply)
        await new Promise((resolve, reject) => {
          process.stdout.write(`${JSON.stringify(reply)}\n`, (error) =>
            error ? reject(error) : resolve(),
          );
        });
    }
  }
} catch (error) {
  console.error(
    JSON.stringify({
      ok: false,
      error:
        error instanceof AgentWorkflowError
          ? { code: error.code, message: error.message }
          : { code: 'PROCESS_ERROR', message: 'Workflow process could not continue.' },
    }),
  );
  process.exitCode = 1;
}
