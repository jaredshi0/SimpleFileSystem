 make:
	gcc -g -c fs.c
	gcc -g -c disk.c
	gcc -o fs fs.o disk.o

clean:
	rm fs.o
	rm disk.o
	rm fs
transfer:
	scp -P 10001 ~/Junior/ec440/hw5/fs.c terrier021@ec440.bu.edu:project5/
	scp -P 10001 ~/Junior/ec440/hw5/fs.h terrier021@ec440.bu.edu:project5/
	scp -P 10001 ~/Junior/ec440/hw5/makefile terrier021@ec440.bu.edu:project5/
	scp -P 10001 ~/Junior/ec440/hw5/README.md terrier021@ec440.bu.edu:project5/