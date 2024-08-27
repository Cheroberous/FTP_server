#!/bin/bash

# Compile server.c
gcc server.c -o s -lcrypt
if [ $? -eq 0 ]; then
    echo "Server compiled successfully."
else
    echo "Error compiling server."
    exit 1
fi

# Compile client.c
gcc client.c -o c 
if [ $? -eq 0 ]; then
    echo "Client compiled successfully."
else
    echo "Error compiling client."
    exit 1
fi

echo "Compilation complete."
