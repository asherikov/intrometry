FIND_SOURCES=find ./test/ ./src ./include/ -iname "*.h" -or -iname "*.cpp"

format:
	${FIND_SOURCES} | xargs ${CLANG_FORMAT} -verbose -i

