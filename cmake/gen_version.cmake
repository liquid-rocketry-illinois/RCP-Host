file(STRINGS ${SOURCE}/VERSION vstr)

if(${BTYPE} STREQUAL "Debug")
    string(APPEND vstr "-DEBUG")
endif()

set(CODESTR "#ifdef __cplusplus\nextern \"C\" {\n#endif\nextern const char* const RCP_VERSION\;\nextern const char* const RCP_VERSION_END\;\nconst char* const RCP_VERSION =\"")
string(APPEND CODESTR ${vstr})
string(APPEND CODESTR "\"\;\nconst char* const RCP_VERSION_END = RCP_VERSION + ")
string(LENGTH ${vstr} vstrlen)
string(APPEND CODESTR ${vstrlen})
string(APPEND CODESTR "\;\n#ifdef __cplusplus\n}\n#endif")
file(WRITE ${BIN}/VERSION.cpp ${CODESTR})
