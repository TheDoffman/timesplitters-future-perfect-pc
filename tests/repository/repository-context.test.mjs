import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {readRepositoryContext} from '../../tools/router/repository-context.mjs';

const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
test('real repository packet finds guest paths and matching system guide, within budget',()=>{
  const packet=readRepositoryContext(root,'Fix Issue 105: FPS, flip-gate, vblank, patch');
  assert.equal(packet.status,'loaded');assert.ok(packet.files.includes('docs/agent-context/runtime.md'));
  assert.match(packet.text,/recomp\/gen/);assert.match(packet.text,/tsfp-patches.json/);assert.ok(packet.bytes<=6500);
  assert.ok(readRepositoryContext(root,'launcher download release').files.includes('docs/agent-context/launcher.md'));
});
test('nested cwd discovers repository; unfamiliar tasks only get compact overview',()=>{
  const packet=readRepositoryContext(path.join(root,'tools'),'xyzquux');
  assert.equal(packet.status,'loaded');assert.deepEqual(packet.files,['docs/agent-context/README.md']);
});

function fixture(t){
  const parent=fs.realpathSync(os.tmpdir()),root=fs.mkdtempSync(path.join(parent,'repo-context-'));
  // Owned fixture contains docs only. Never target an existing checkout.
  t.after(()=>{
    assert.equal(path.dirname(root),parent);
    assert.match(path.basename(root),/^repo-context-[\w-]+$/);
    assert.equal(fs.realpathSync(root),root);
    fs.rmSync(root,{recursive:true,force:true});
  });
  fs.mkdirSync(path.join(root,'.git'));fs.writeFileSync(path.join(root,'AGENTS.md'),'Fixture');
  fs.mkdirSync(path.join(root,'docs/agent-context'),{recursive:true});
  return root;
}
test('absent or malformed context cannot crash another repository',t=>{
  const root=fixture(t);assert.equal(readRepositoryContext(root).status,'absent');
  fs.writeFileSync(path.join(root,'docs/agent-context/manifest.json'),'{invalid');
  assert.equal(readRepositoryContext(root).status,'invalid');
});
test('manifest cannot request private files or arbitrary topics',t=>{
  const root=fixture(t);
  fs.writeFileSync(path.join(root,'docs/agent-context/manifest.json'),JSON.stringify({version:1,overview:'docs/agent-context/README.md',topics:[{id:'runtime',file:'private/data',keywords:['fix']}]}));
  assert.equal(readRepositoryContext(root,'fix').status,'invalid');
});
test('junctions/symlinks in the context path are rejected',t=>{
  const root=fixture(t),outside=fixture(t),doc=path.join(root,'docs/agent-context');
  fs.rmdirSync(doc);
  fs.symlinkSync(path.join(outside,'docs/agent-context'),doc,process.platform==='win32'?'junction':'dir');
  fs.writeFileSync(path.join(outside,'docs/agent-context/manifest.json'),'{}');
  assert.equal(readRepositoryContext(root).status,'invalid');
});
test('Unicode context bytes remain bounded and overlarge excerpts are rejected',t=>{
  const root=fixture(t),doc=path.join(root,'docs/agent-context');
  fs.writeFileSync(path.join(doc,'manifest.json'),JSON.stringify({version:1,overview:'docs/agent-context/README.md',topics:[]}));
  fs.writeFileSync(path.join(doc,'README.md'),'<!-- packet:start -->'+ '\u754c'.repeat(1100)+'<!-- packet:end -->');
  assert.equal(readRepositoryContext(root).status,'invalid');
});
