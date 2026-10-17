const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const source = fs.readFileSync(`${__dirname}/../qml/Main.qml`, 'utf8');
const functions = source.match(/^    function \w+\([^\n]*\) \{[^\n]*\}$|^    function \w+\([^\n]*\) \{[\s\S]*?^    \}/gm);
const calls = [];
const state = {root:null, jobsOpen:false, enableGamepadActions:true, keyboardOpen:false, modal:'',
  modalChoice:0, pendingJobId:'', pendingId:'', selectedGame:null, detailsOpen:false, galleryFullscreen:false,
  focusArea:'catalog', store:{busy:true, activeJob:'first-job', cancelJob:id=>calls.push(['cancel',id]),retryJob:id=>calls.push(['retry',id])},
  close:()=>calls.push(['close']), activityPage:{input:action=>calls.push(['activity',action])},
  shelf:{positionViewAtBeginning:()=>{}}, navIndex:0, section:'Discover', selectedIndex:0};
state.root=state;
vm.createContext(state); vm.runInContext(functions.join('\n'),state);
state.leaveStore(); assert.deepEqual(calls,[['close']],'window close must not request cancellation');
state.requestCancel('second-job'); assert.equal(state.pendingJobId,'second-job');
assert.equal(state.modal,'cancel'); assert.equal(calls.length,1,'cancel requires confirmation');
state.modalChoice=1;state.executeModalChoice();assert.deepEqual(calls.at(-1),['cancel','second-job']);
state.jobsOpen=true;state.handleAction('back');assert.deepEqual(calls.at(-1),['activity','back']);
state.jobsOpen=false;state.changeSection('Activity');assert.equal(state.jobsOpen,true);
console.log('PASS: background window closure, stable cancellation target, confirmation and Activity input isolation.');
