# Towncrier

A server and client that alerts me when I need to update my backups

### To Run
We use [nob.h](https://github.com/tsoding/nob.h) as our build system here. To build:
```console
cc nob.c -o nob
./nob
```

On a server, run:
```console
$ nohup ./build/towncrier 2>> tc.log &
```

to write to a log file and run in the background, then add the client 
`peasant` to your bashrc.
