#NAME: Jorge Contreras, Nevin Liang
#EMAIL: jorgec9000@g.ucla.edu, nliang868@g.ucla.edu
#ID: 205379811, 705575353

lab3b:
	touch lab3b
	echo "#! /bin/bash" > lab3b
	echo -e "python3 main.py \$$1" >> lab3b
	chmod u+x lab3b

dist: README Makefile main.py
	tar -czvf lab3b-205379811.tar.gz README Makefile main.py

clean: 
	rm -f lab3b-205379811.tar.gz lab3b
