rm valgrind.log
valgrind --track-origins=yes --leak-check=full --trace-children=yes --track-fds=yes --time-stamp=yes --log-file=valgrind.log ./bin/homecam

