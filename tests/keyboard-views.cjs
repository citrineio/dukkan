const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const qml = fs.readFileSync(path.join(__dirname, '../qml/Main.qml'), 'utf8');
const rows = qml.match(/readonly property var keyRows: ([\s\S]*?)\n    readonly property string keyboardViewLabel/);
assert.ok(rows, 'keyboard rows must be readable from the actual QML');
function grid(view) { return vm.runInNewContext(rows[1], {keyboardView:view}); }
for (let view=0; view<3; view++) {
  const keys = grid(view);
  assert.equal(keys.length, 4);
  keys.forEach(row => assert.equal(row.length, 10));
}
assert.deepEqual(Array.from(grid(0)[0]), Array.from('qwertyuiop'));
assert.deepEqual(Array.from(grid(1)[0]), Array.from('QWERTYUIOP'));
assert.deepEqual(Array.from(grid(2)[0]), Array.from('!@#$%^&*()'));
for (const key of ['-', '_', '.', '/', '?', '@', '\\'])
  assert.ok(grid(2).flat().includes(key), `symbol ${key} is available`);

const keyStart = qml.indexOf('    function keyActivate(label) {');
const cycleStart = qml.indexOf('    function cycleKeyboardView()', keyStart);
assert.ok(keyStart >= 0 && cycleStart > keyStart);
const state = vm.createContext({keyboardView:0, searchQuery:'', selectedIndex:0,
  shelf:{positionViewAtBeginning:()=>{}}, closeKeyboard:()=>{}});
vm.runInContext(qml.slice(keyStart, cycleStart), state);
vm.runInContext(qml.slice(cycleStart, qml.indexOf('\n    function keyMove(', cycleStart)), state);
for (const expected of [1, 2, 0]) {
  vm.runInContext('cycleKeyboardView()', state);
  assert.equal(state.keyboardView, expected);
}
state.keyActivate('Q');
state.keyActivate('@');
state.keyActivate('SPACE');
state.keyActivate('z');
assert.equal(state.searchQuery, 'Q@ z', 'input preserves selected case and symbols');
state.keyActivate('⌫');
assert.equal(state.searchQuery, 'Q@ ');
console.log('PASS: Dukkan lower, upper, and symbol keyboard views and input.');
