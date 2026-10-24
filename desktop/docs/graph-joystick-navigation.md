# Graph joystick navigation

The joystick moves the viewport in the tilt direction. Right looks toward the right side of the graph (`scrollLeft` increases), so visible node content moves left. Up looks toward the top (`scrollTop` decreases), so content moves down. The knob continues to track the finger. This is viewport navigation, unlike grabbing content with ordinary drag-pan.

Only the joystick-to-viewport sign changes. Normal drag-pan, wheel, Overview, node coordinates, document/history and Canvas controls retain their existing behavior. Speed, deadzone, fractional accumulation, touch capture, release/Cancel/Escape, focus/IME guards and origin clamping are retained.

Directional regressions cover all four axes and four diagonals through the shipped app's event path, plus the real-browser suite's viewport offsets and rendered node rectangles. Controller tests retain mouse/touch, deadzone and stop-path coverage. DOM fixtures do not establish real-browser or native visual acceptance; record those executions separately. The supplied user screenshot was not inspected because supported Library materialization failed; the later explicit user clarification establishes viewport semantics independently.

From desktop:

```sh
node --test tests/graph-pan-joystick.test.js tests/graph-pan-joystick-dom.test.js tests/graph-pan-joystick-setup.test.js tests/graph-pan-joystick-app-dom.test.js tests/view-navigation.test.js
```

The existing browser suite includes graph-pan-joystick.e2e.js. No native app, existing user document, Resolve, updater, installation or release is part of this fix.
