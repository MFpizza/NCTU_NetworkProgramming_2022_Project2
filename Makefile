main:
	make clean
	g++ np_simple_source/np_simple.cpp -o np_simple
	g++ -w -g np_single_proc_source/np_single_proc.cpp -o np_single_proc 
	g++ -w -g np_multi_proc_source/np_multi_slave.cpp np_multi_proc_source/np_multi_proc.cpp -o np_multi_proc
clean:
	rm -f np_simple
	rm -f np_single_proc
	rm -f np_multi_proc