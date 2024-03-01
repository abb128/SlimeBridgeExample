example: example.cpp
	gcc example.cpp Bridge.cpp ProtobufMessages.pb.cc -Wl,--copy-dt-needed-entries -lprotobuf -g -o ./example
