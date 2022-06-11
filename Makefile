all:
	gcc -g main.c cmd.c filesystem.c -o hw3
clean:
	rm -rf hw3
