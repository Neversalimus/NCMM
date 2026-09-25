# Repository migration

This standalone repository was derived from the NCMM history previously stored under
`ncmm-platform/` in `Neversalimus/NCMM`.

Source cutover commit: `5fd2968a959753170deb83752ef3d031e0790daf`

The migration replayed only commits that changed `ncmm-platform/` and used that directory's
tree as the root tree for each replayed commit. This preserves the meaningful NCMM development
history while removing the unrelated legacy history inherited from `Whales/Cataclysm`.

GitHub Actions workflows lived outside `ncmm-platform/` in the old repository, so their current
0.7.0 versions were imported in the final repository-migration commit and rewritten for the new
root layout.

The standalone repository uses main as its default branch. Its feed is intentionally reset to an
empty NCMM 0.7.0 feed at migration time so Certified Hosts can republish trusted assets under the
new repository identity.

The old repository should remain available until Runtime, Feed Integrity and Certified Hosts all
pass from this standalone repository.
