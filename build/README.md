Built app bundles will be placed here. 

To build for simulator, run `make` in the parent directory.

To build for device, run `make` in the parent directory, specifying DEVICE=1, along with a provisioning profile and (optionally) a code-sign identity if it would otherwise be ambiguous. e.g.,

```bash
make DEVICE=1 \
  PROVISIONING_PROFILE=~/path/to/profile.mobileprovision \
  CODESIGN_IDENTITY="Apple Development: You (TEAMID)" 00-window
```

Note that because provisioning profiles are specific to the app bundle identifier, you should not run `make` with DEVICE=1 without specifying a specific target.

Also note that object files are not platform-tagged, so run `make clean` when switching between simulator and device — otherwise stale object files of the wrong architecture might get relinked.
