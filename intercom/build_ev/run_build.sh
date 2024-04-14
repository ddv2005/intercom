sudo docker run -it --rm --name=intercom_buildev_deb12_run \
 --mount type=bind,src=/projects,target=/projects \
 intercom_buildev_deb12 \
 $(pwd)/build.sh