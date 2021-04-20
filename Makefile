MAIN := ps
SRC := ./*.c

${MAIN} : ${SRC}
	gcc $^ -o $@ 
clean:
	rm ${MAIN}
