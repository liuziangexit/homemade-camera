mkdir -p libs
cp -ru /opt/vc/src/hello_pi/libs/ilclient libs
cp -u /opt/vc/src/hello_pi/Makefile.include .
make -C libs/ilclient
cp libs/ilclient/libilclient.a .
cp libs/ilclient/ilclient.h .
