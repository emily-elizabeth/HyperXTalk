# Contributing to HyperXTalk

## Contributors' forums

General discussion about contributing to the HyperXTalk open-source projects takes place on the [HyperXTalk forum](https://hyperxtalk.discourse.group), and in particular the [Compiling the engine category](https://hyperxtalk.discourse.group/c/compiling-the-engine/8).

## Using GitHub

The HyperXTalk workflow is a typical git workflow, where contributors fork the [emily-elizabeth/HyperXTalk](https://github.com/emily-elizabeth/HyperXTalk) repository, make their changes on a branch, and then submit a pull request.

### Creating a pull request

When you submit a pull request, please make sure to follow the following steps:

1. Ensure that all the commits have good log messages, in the following format:

  ```
  Summary line of less than 80 characters

  Explanation of what the commit fixes and why it's the right
  fix, possibly using multiple paragraphs.  For example, you
  might want to describe other options and why the one you
  chose is better.
  ```

  It's very important that readers can get a good idea of what the commit is about just by reading the summary line.  To help with this, we use some special "tags" at the start of a commit message summary line:

  * If the commit fixes a bug, please add `[[Bug <issue number>]]` at the start.

  * If the commit relates to a particular new feature — and there are several commits and pull requests involved in the feature — please add `[<feature-name>]` at the start.

2. Make sure that the pull request only relates to *one* change (one bug fix, one new feature, etc.) or to a group of very closely-related fixes.  Please make sure that the pull request has a good description too (often you can base the title and body of the pull request on the commit messages).

  Please highlight any areas of your changes that you thought were particularly difficult to figure out.  This will help make sure that your code gets thoroughly reviewed.

### Pull request process

After you submit a pull request, a member of the HyperXTalk team will review your changes.  They will probably find some improvements that need to be made.  Please note that if a reviewer asks you to change your code, it doesn't necessarily mean that there was anything wrong with the changes you've made.  It often means that they have spotted a way to fix other things at the same time, or to make your change fit in better with other things that are being worked on elsewhere.

Once a reviewer is happy with the changes, they will mark the pull request as reviewed.  In the future, the HyperXTalk continuous integration system will then take your code, and automatically build & test it on all of the platforms supported by HyperXTalk.  Currently the testing is being done manually.

If the tests don't pass, then you will need to make some more changes to fix the problems that were found.  These will then be reviewed, etc.

Once your changes have been reviewed and tested, they will be merged in time for the next release.

## Bugs

Finding and fixing bugs in HyperXTalk is a particularly valuable contribution.

If you've found a bug, please add a ticket to the [GitHub project issue tracking system](https://github.com/emily-elizabeth/HyperXTalk/issues).  This will give you an issue number which can be used whenever discussing the issue, and included in git commit log messages and in GitHub pull request descriptions.  This will help other contributors keep track of who's working on what.

When you submit a pull request that fixes a bug, the status of the bug should be set to "AWAITING_MERGE" -- please also add a comment to the PR with a link to the issue (Closes #___).

When the pull request is merged, the status should be set to "AWAITING_BUILD".

## Coding style

See the separate documentation for:

- [C++ coding style](docs/development/C++-style.md) and
  [use of C++ language features](docs/development/C++-features.md)

- [HyperXTalk Builder coding style](docs/guides/Builder%20Style%20Guide.md)

## Documentation

Improving the documentation of HyperXTalk is another way of helping the project.  See [contributing to docs](docs/contributing_to_docs.md).