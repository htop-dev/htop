HTOP the Docker Way
---
To run HTOP in a docker container,

* Build the docker image
```sh
wget https://raw.githubusercontent.com/htop-dev/htop/master/docker/Dockerfile
docker build -t htop .

# Run the image using
docker run --rm -it --pid=host htop
```
### -OR-
* Use a pre-built docker image (**easier way**)
```sh
docker run --rm -it --pid=host snpranav/htop
```

This  will pull the docker image from [docker hub](https://hub.docker.com/snpranav/htop) and run htop in a docker container.
