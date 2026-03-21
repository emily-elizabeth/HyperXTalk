# LiveCode Thirdparty Libraries

The procedure for updating `thirdparty` on `develop-9.0` and `develop` branches is
slightly different due to `thirdparty` prebuilts on develop. 

## Update procedure for develop-9.0

1. Push up a PR to this repo
2. If there is a related patch to another repos then push up a PR for those
3. Create a test branch on `livecode` with updated submodule ptrs. Ensure the
branch is pushed upstream rather than to your clone.
4. If it was a CEF update then push up a branch to `livecode-private` again with 
updated submodule and kick off prebuilts on vulcan.
5. Create a PR with the title beginning with `[[ NO MERGE ]]` and review it ok
to confirm it passes on travis and vulcan
6. Close PR from 5 and link to it in the other related PRs
7. Once all related PRs are reviewed merge `thirdparty`, update submodules in
`livecode` and merge any related PRs

## Update procedure for develop

The same procedure applies to merge-ups from `develop-9.0` because the 
`thirdparty` prebuilts must be rebuilt for the current head of the `develop`
branch.

1. Push up a PR to this repo
2. If there is a related patch to another repos then push up a PR for those
3. Review and merge the PR for `thirdparty` but *do not* update submodule ptrs
in `livecode` yet.
4. Create a test branch on `livecode` with updated submodule ptrs. Ensure the
branch is pushed upstream rather than to your clone.
5. Push up a branch to `livecode-private` again with updated submodule and kick
off prebuilts on vulcan.
6. Once prebuilts are built for the `develop` head of `thirdparty` create a PR
using the `livecode` branch with the title beginning with `[[ NO MERGE ]]` and
review it ok to confirm it passes on travis and vulcan
7. Close PR from 6 and link to it in the other related PRs
8. Once all related PRs are reviewed, update submodules in `livecode` and merge
any related PRs
