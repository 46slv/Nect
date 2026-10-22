import { PropertySymbol } from 'happy-dom';

/**
 * Native MutationObservers keep an observed listener live. Happy DOM 20.8.9
 * stores its delivery callback only in a WeakRef, so GC during font loading or
 * a structural render can silently stop the real command-menu observer.
 * Retain those callbacks while observed, without replacing mutation delivery.
 * This workaround is confined to the Pen/menu test window.
 */
export function penMenuMutationObserver(NativeMutationObserver) {
  return class extends NativeMutationObserver {
    #callbacks = new Map();

    observe(target, options) {
      const previous = new Set(target?.[PropertySymbol.mutationListeners]);
      super.observe(target, options);
      for (const listener of target[PropertySymbol.mutationListeners]) {
        if (!previous.has(listener)) this.#callbacks.set(listener, listener.callback.deref());
      }
    }

    disconnect() {
      super.disconnect();
      this.#callbacks.clear();
    }

    get retainedCallbackCount() {
      return this.#callbacks.size;
    }
  };
}
