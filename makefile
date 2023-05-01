all:
	g++ server.cpp co_event.cpp coroutine.cpp -o server
	g++ client.cpp co_event.cpp coroutine.cpp -o client
	g++ test_sleep_hook.cpp co_event.cpp coroutine.cpp -o sleep_hook_test
	

