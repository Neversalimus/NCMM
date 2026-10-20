'use strict';

const objectId = value => typeof value === 'string' && /^[0-9a-f]{40}$/.test(value);

function entries(tree) {
  if (!tree || tree.truncated !== false || !objectId(tree.sha) || !Array.isArray(tree.tree)) {
    throw new Error('Missing or truncated Git tree; qualification must fail closed.');
  }
  const paths = new Map();
  for (const entry of tree.tree) {
    if (!entry || typeof entry.path !== 'string' || !entry.path || entry.path.startsWith('/') ||
        entry.path.split('/').some(part => part === '..' || part === '' || part === '.') ||
        entry.path.includes('\0') || !objectId(entry.sha) || !/^[0-7]{6}$/.test(entry.mode) ||
        !['tree', 'blob', 'commit'].includes(entry.type) || paths.has(entry.path)) {
      throw new Error('Invalid Git tree entry; qualification must fail closed.');
    }
    paths.set(entry.path, entry);
  }
  return new Map([...paths].filter(([, entry]) => entry.type !== 'tree'));
}

function changedPaths(before, after) {
  const a = entries(before), b = entries(after);
  return [...new Set([...a.keys(), ...b.keys()])].filter(path => {
    const left = a.get(path), right = b.get(path);
    return !left || !right || left.sha !== right.sha || left.mode !== right.mode || left.type !== right.type;
  }).sort();
}

async function completeChangedPaths(github, repo, base, head) {
  if (!objectId(base) || !objectId(head)) throw new Error('Exact commit identities required.');
  async function read(commitSha) {
    const commit = (await github.rest.git.getCommit({...repo, commit_sha: commitSha})).data;
    if (commit.sha !== commitSha || !commit.tree || !objectId(commit.tree.sha)) throw new Error('Git commit identity differs.');
    const tree = (await github.rest.git.getTree({...repo, tree_sha: commit.tree.sha, recursive: '1'})).data;
    if (tree.sha !== commit.tree.sha) throw new Error('Git tree identity differs.');
    return tree;
  }
  const [before, after] = await Promise.all([read(base), read(head)]);
  return changedPaths(before, after);
}

module.exports = {changedPaths, completeChangedPaths};
