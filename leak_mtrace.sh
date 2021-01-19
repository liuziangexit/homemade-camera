#valgrind --leak-check=yes bin/homecam 
export MALLOC_TRACE="mtrace.log"
./bin/homecam
mtrace bin/homecam mtrace.log