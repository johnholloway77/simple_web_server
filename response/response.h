#ifndef RESPONSE_H
#define RESPONSE_H

#define RESPONSE_200                                                           \
	"HTTP/1.0 200 Ok\r\nContent-Type: %s\r\nConnection: close\r\n\r\n"

#define RESPONSE_400                                                           \
	"HTTP/1.0 400 Bad Request\r\n"                                         \
	"Content-Length: 0\r\n"                                                \
	"Connection: close\r\n\r\n"

#define RESPONSE_403                                                           \
	"HTTP/1.0 403 Forbidden\r\n"                                           \
	"Connection: close\r\n\r\n"                                            \
	"Forbidden"

#define RESPONSE_404                                                           \
	"HTTP/1.0 404 Not Found\r\n"                                           \
	"Content-Type: text/html\r\n"                                          \
	"Connection: close\r\n\r\n"                                            \
	"\r\n"                                                                 \
	"<html><body><h1>404 Not Found</h1>"                                   \
	"<h3>Sorry, the file you are looking for doesn't exist</h3>"           \
	"<br><br><p>Or does it?!?!</p><br><br><br><br>"                        \
	"<p>No... it doesn't</p></body></html>\r\n"

#define RESPONSE_500                                                           \
	"HTTP/1.0 500 Internal Error\r\n"                                      \
	"Content-Length: 0\r\n"                                                \
	"Connection: close\r\n\r\n"

#define RESPONSE_501                                                                                \
	"HTTP/1.0 501 Not Implemented\r\n"                                                          \
	"Connection: close\r\n"                                                                     \
	"\r\n"                                                                                      \
	"<html><body><h1>501 CGI Not Enabled on Server </h1>"                                       \
	"<h3>CGI application requires the server to be run with the CGI flag</h3>"                  \
	"<p>Please restart the simple_server application with the -c flag and the location of the " \
	"cgi-bin directory</p>"                                                                     \
	"<p><b>Eg:<br/></b> ./simple_server -c ./cgi-bin\n</p></body></html>\r\n"

#define RETURN_RESP(x)                                                         \
	char *response;                                                        \
	response = (char *)malloc(strlen(x) + 1);                              \
	strlcpy(response, x, strlen(x) + 1);                                   \
	return response;

#endif // RESPONSE_H
