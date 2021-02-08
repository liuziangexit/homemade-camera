./config --prefix="$(pwd)/sslinstall" --openssldir="$(pwd)/sslinstall"
make -j4
make install