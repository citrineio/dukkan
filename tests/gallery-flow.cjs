// Execute the actual QML controller state functions with an isolated store.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const qml = fs.readFileSync(`${__dirname}/../qml/Main.qml`, 'utf8');
function source(name) {
  const start = qml.indexOf(`    function ${name}(`);
  assert.ok(start >= 0, name);
  let end = qml.indexOf('\n    function ', start + 1);
  // handleAction is followed by Connections, not another function.
  const connections = qml.indexOf('\n    Connections ', start + 1);
  if (end < 0 || (connections >= 0 && connections < end)) end = connections;
  return qml.slice(start, end);
}
let actionCalls = [];
const ctx = vm.createContext({
  enableGamepadActions:true, keyboardOpen:false, modal:'', modalChoice:0,
  jobsOpen:false, selectedPending:false, root:null,
  galleryFullscreen:false, galleryIndex:0, galleryImages:[], detailsOpen:true,
  detailAction:1, focusArea:'details', selectedGame:{id:'test', installed:true},
  store:{busy:false}, px:n=>n, descriptionFlick:{contentY:145, contentHeight:900, height:400},
  thumbnails:{positionViewAtIndex:()=>{}}, ListView:{Contain:0},
  requestGameAction:()=>actionCalls.push('play'), requestUninstall:()=>actionCalls.push('remove'),
  changeSection:()=>actionCalls.push('section'), keyMove:()=>{},
  requestCancel:()=>actionCalls.push('cancel'), openKeyboard:()=>actionCalls.push('search'),
  dismissModal:()=>{ctx.modal='';}, executeModalChoice:()=>actionCalls.push('confirm')
});
ctx.root=ctx;
for (const name of ['moveGallery','openGallery','handleAction']) vm.runInContext(source(name), ctx);
ctx.openGallery();
assert.equal(ctx.galleryFullscreen,false,'empty gallery cannot open');
ctx.galleryImages=[{url:'one'}];
ctx.openGallery();
ctx.handleAction('nextImage');
assert.equal(ctx.galleryIndex,0,'single image stays in place');
ctx.handleAction('back');
ctx.galleryImages=[{url:'one'},{url:'two'},{url:'three'}];
ctx.handleAction('previousImage');
assert.equal(ctx.galleryIndex,2,'L1 wraps to final image');
ctx.handleAction('nextImage');
assert.equal(ctx.galleryIndex,0,'R1 wraps to first image');
ctx.handleAction('library');
assert.equal(ctx.galleryFullscreen,true,'Y opens viewer within details');
for(const action of ['select','library','refresh','search','left','right','up','down']) ctx.handleAction(action);
assert.equal(ctx.detailAction,1);
assert.deepEqual(actionCalls,[],'viewer cannot activate underlying game/store actions');
assert.equal(ctx.descriptionFlick.contentY,145);
ctx.handleAction('nextImage');
ctx.handleAction('back');
assert.equal(ctx.galleryFullscreen,false);
assert.equal(ctx.detailsOpen,true,'first B closes only viewer');
assert.equal(ctx.galleryIndex,1);
assert.equal(ctx.detailAction,1,'return preserves uninstall selection');
ctx.handleAction('select');
assert.deepEqual(actionCalls,['remove']);
ctx.handleAction('back');
assert.equal(ctx.detailsOpen,false,'second B closes detail page');
ctx.detailsOpen=true;
ctx.modal='remove';
ctx.handleAction('nextImage');
assert.equal(ctx.galleryIndex,1,'confirmation owns input');
ctx.modal='';
ctx.enableGamepadActions=false;
ctx.handleAction('nextImage');
assert.equal(ctx.galleryIndex,1,'inactive store ignores controller input');
console.log('PASS: empty/single/multiple gallery, wrap, fullscreen isolation, B return, and focus guards.');
