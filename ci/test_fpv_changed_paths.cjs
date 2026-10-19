'use strict';
const {test} = require('node:test');
const assert = require('node:assert/strict');
const {changedPaths, completeChangedPaths} = require('./fpv_changed_paths.cjs');
const sha = letter => letter.repeat(40);
const file = (path, value = 'a', mode = '100644', type = 'blob') => ({path, sha: sha(value), mode, type});
const tree = items => ({sha: sha('e'), truncated: false, tree: items});

test('all 350 changed files survive the compare API 300-file limit', () => {
  const before = tree(Array.from({length: 350}, (_, i) => file(`source/${i}.cpp`)));
  const after = tree(Array.from({length: 350}, (_, i) => file(`source/${i}.cpp`, 'b')));
  assert.equal(changedPaths(before, after).length, 350);
});
test('additions, deletions, mode-only changes, symlinks and gitlinks are covered', () => {
  const before = tree([file('deleted'), file('executable'), file('link', 'a', '120000'), file('submodule', 'a', '160000', 'commit')]);
  const after = tree([file('added'), file('executable', 'a', '100755'), file('link', 'b', '120000'), file('submodule', 'b', '160000', 'commit')]);
  assert.deepEqual(changedPaths(before, after), ['added', 'deleted', 'executable', 'link', 'submodule']);
});
test('renames retain both names and unchanged files stay absent', () => {
  assert.deepEqual(changedPaths(tree([file('old'), file('same')]), tree([file('new'), file('same')])), ['new', 'old']);
});
test('directory entries are ignored while Unicode, spaces and newline names survive JSON', () => {
  const path = 'source/имя с пробелом\nand-newline.cpp';
  const paths = changedPaths(tree([file('source', 'a', '040000', 'tree')]), tree([file('source', 'b', '040000', 'tree'), file(path)]));
  assert.deepEqual(JSON.parse(JSON.stringify(paths)), [path]);
});
test('truncated or missing trees fail closed', () => {
  for (const value of [null, {}, {...tree([]), truncated: true}, {...tree([]), tree: null}]) {
    assert.throws(() => changedPaths(value, tree([])), /truncated/);
  }
});
test('duplicate and malformed entries fail closed', () => {
  for (const items of [[file('same'), file('same')], [file('../escape')], [file('/absolute')], [{...file('bad'), sha: 'unknown'}], [file('bad', 'a', 'invalid')]]) {
    assert.throws(() => changedPaths(tree(items), tree([])), /Invalid/);
  }
});
test('API commits and trees remain bound to the exact requested identities', async () => {
  const api = {rest: {git: {
    getCommit: async ({commit_sha}) => ({data: {sha: commit_sha, tree: {sha: sha(commit_sha === sha('a') ? 'c' : 'd')}}}),
    getTree: async ({tree_sha}) => ({data: {...tree(tree_sha === sha('c') ? [] : [file('sdk/api.h')]), sha: tree_sha}})
  }}};
  assert.deepEqual(await completeChangedPaths(api, {owner: 'fixture', repo: 'fixture'}, sha('a'), sha('b')), ['sdk/api.h']);
  api.rest.git.getCommit = async () => ({data: {sha: sha('f'), tree: {sha: sha('c')}}});
  await assert.rejects(completeChangedPaths(api, {}, sha('a'), sha('b')), /identity/);
});
