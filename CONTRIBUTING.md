# Contributing to Hazel 

If you don't already have a GitHub account, kindly create one. Your GitHub username will be referred to later as 'YOUR_GITHUB_USERNAME'. Change it accordingly in the steps below.

1. Fork https://github.com/hazel-lang/Hazel using GitHub's interface to your own account.
Let's say that the forked repository is at
`https://github.com/YOUR_GITHUB_USERNAME/v` .
2. Clone the [main Hazel repository](https://github.com/hazel-lang/Hazel) to a local folder on your computer, say named `Hazel`
```bash
git clone https://github.com/hazel-lang/Hazel Hazel
```

3. `cd Hazel`

4. `git remote add pullrequest https://github.com/YOUR_GITHUB_USERNAME/Hazel`
NB: the remote named `pullrequest` should point to YOUR own forked repo, not the main v repository! After this, your local 
cloned repository is prepared for making pullrequests, and you can just do normal git operations such as: 
`git pull`, `git status` and so on.

5. When finished with a feature/bugfix/change, you can:
`git checkout -b fix_alabala`

6. `git push pullrequest`  # (NOTE: the `pullrequest` remote was setup on step 4)

7. On GitHub's web interface, go to: https://github.com/hazel-lang/Hazel/pulls

   Here the UI shows a dialog with a button to make a new pull request based on
   the new pushed branch.
   (Example dialog: https://url4e.com/gyazo/images/364edc04.png)

8. After making your pullrequest (aka, PR), you can continue to work on the
branch `fix_alabala` ... just do again `git push pullrequest` when you have more
commits.

9. If there are merge conflicts, or a branch lags too much behind Hazel's master,
you can do the following:

   1. `git pull --rebase origin master` # solve conflicts and do
   `git rebase --continue`
   2. `git push pullrequest -f` # this will overwrite your current remote branch
   with the updated version of your changes.

The point of doing the above steps, is to never directly push to the main Hazel repository, *only to your own fork*. Since 
your local `master` branch tracks the main Hazel repository's master, then `git checkout master`, as well as 
`git pull --rebase origin master` will continue to work as expected (these are actually used by `hazel up`) and git can always do it cleanly.

Git is very flexible, so there are other ways to accomplish the same thing.
See the [GitHub flow](https://guides.github.com/introduction/git-handbook/#github), for more information.


## Using Github's hub CLI tool

You can download the `hub` tool from https://hub.github.com/ . Using `hub`, you will not need to go through the (sometimes) 
slow website to make PRs. Most remote operations can be done through the `hub` CLI command.
NB: You still need to have a GitHub account.


### Preparation:
(steps 1..3 need to be done just *once*):

1. `hub clone hazel-lang/Hazel Hazel`
2. `cd Hazel`
3. `hub fork --remote-name pullrequest`
4. `git checkout -b my_cool_feature` # Step 4 is better done *once per each new feature/bugfix* that you make.
5. `git commit -am "math: add a new function copysign"` (make a commit)


### Testing your commits locally:
You can test locally whether your changes have not broken something by running: `hazel test-all`. See `TESTS.md` for more 
details.

### Publishing your commits to GitHub:

6. `git push pullrequest`


### Making a PR with `hub`:
(so that your changes can be merged to the main Hazel repository)

7. `hub pull-request`

Optionally, you can track the status of your PR CI tests with:

8. `hub ci-status --verbose`


### Fixing failing tests:
If everything is OK, after 5-10 minutes, the CI tests should pass for
all platforms. If not, visit the URLs for the failing CI jobs, see
which tests have failed and then fix them by making more changes. Just use
`git push pullrequest` to publish your changes. The CI tests will
run with your updated code. Use `hub ci-status --verbose` to monitor
their status.


<!-- ## Flags

Hazel allows you to pass custom flags using `-d my_flag` that can then be checked
at compile time (see the documentation about
[compile-time if](https://github.com/hazel-lang/Hazel/blob/master/doc/docs.md#compile-time-if)).
There are numerous flags that can be passed when building the compiler
with `hazel self` or when creating a copy of the compiler, that will help
you when debugging.

Beware that the flags must be passed when building the compiler,
not the program, so do for example: `hazel -d time_parsing cmd/hazel` or
`hazel -d trace_checker self`.
Some flags can make the compiler very verbose, so it is recommended
to create a copy of the compiler rather than replacing it with `hazel self`. -->
<!-- 
| Flag | Usage |
|------|-------|
| `debugautostr` | Prints informations about `.str()` method auto-generated by the compiler during C generation |
| `debugscanner` | Prints debug information during the scanning phase |
| `debug_codegen` | Prints automatically generated Hazel code during the scanning phase |
| `debug_interface_table` | Prints generated interfaces during C generation |
| `debug_interface_type_implements` | Prints debug information when checking that a type implements in interface |
| `print_vweb_template_expansions` | Prints vweb compiled HTML files |
| `time_checking` | Prints the time spent checking files and other related informations |
| `time_parsing` | Prints the time spent parsing files and other related informations |
| `trace_ccoptions` | Prints options passed down to the C compiler |
| `trace_checker` | Prints informations about the statements being checked |
| `trace_gen` | Prints strings written to the generated C file. Beware, this flag is very verbose |
| `trace_parser` | Prints informations about parsed statements and expressions |
| `trace_thirdparty_obj_files` | Prints informations about built thirdparty obj files |
| `trace_use_cache` | Prints informations about cache use | -->