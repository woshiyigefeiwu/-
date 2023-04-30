all:
	g++ server.cpp coroutine.cpp -o server -l event	
	g++ client.cpp coroutine.cpp -o client -l event
	g++ test_sleep_hook.cpp coroutine.cpp -o sleep_hook_test -l event

