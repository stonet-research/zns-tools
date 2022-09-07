FILEVIZ=filemap

.PHONY: all clean

all: ${FILEVIZ}.c
	gcc ${FILEVIZ}.c -o ${FILEVIZ}	

clean:
	rm ${FILEVIZ}
