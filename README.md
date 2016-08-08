Build instructions for the impatient

Make sure the [centreon-clib](https://github.com/centreon-deb/centreon-clib)
development libs are installed first.

```bash
curl https://raw.githubusercontent.com/centreon-deb/centreon-engine/debian/bootstrap | sh
cd centreon-engine && git deb-pkg -C -U -u 1.5.0 -d origin/debian build
```

Further instruction in the [README.Build.md](README.Build.md) file.
