scsc_sprint1
============

Swinburne Cyber Security Club code sprint #1 chat server.


# Maintainers
## Daniel Parker
* Email: [dparker.tech@gmail.com](mailto:dparker.tech@gmail.com)
* github: rlgod
* Website: [http://danielparker.me](http://danielparker.me/)

# Contributing
## 1. Create a Branch
To begin developing you should create a branch of your own so that you're not interfering with anybody elses code.

## 2. Verbose Commit Messages
Your commit messages should be as verbose as possible. If your commits haven't got at least 2 large sentences of explanation then they aren't big enough and won't be accepted into master.

See ["Writing Good Commit Messages"](https://github.com/erlang/otp/wiki/Writing-good-commit-messages)

## 3. Create Patches NOT Pull Requests
All pull requests will be deleted as the correct way to get your code into master is to create a patch and email it to a core maintainer (see list at the top of this document).

# Patches
## Creating Patches
1. Commit locally
```
git commit -m "Verbose commit message"
```
2. Create a patch with reference to master (you should make sure your copy of master is up to date before doing this)
```
git format-patch master --stdout > your_patch_name.patch
```

## Applying a Patch
1. Obtain the .patch file to apply
2. View the patch stats (optional)
```
git apply --stat your_patch_name.patch
```
3. Check to see if applying the patch will cause any issues
```
git apply --check your_patch_name.patch
```
4. Fully apply the patch (with signoff)
```
git am --signoff < your_patch_name.patch
```

# Building
Please use the makefile in the root directory to build the program if you can. All extra gcc flags should be added to the makefile if necessary so that all developers use it when building. The binary is created in the ```bin/``` directory.

Build:
```
make
```

Clean:
```
make clean
```
