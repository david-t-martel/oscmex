// osc_rme.cpp

#include "mex.h"
#include "lo/lo.h"
#include "oscmix.h"
#include <winsock2.h> // For Windows sockets

#define OSC_BUFFER_SIZE 1024

// Function to open an OSC connection (updated for localhost and dynamic ports)
void osc_open(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[]) {
  // Check for proper number of arguments
  if (nrhs != 0) {
    mexErrMsgIdAndTxt("osc:open:nrhs", "No inputs required.");
  }
  if (nlhs != 3) { // Updated to return 3 outputs
    mexErrMsgIdAndTxt("osc:open:nlhs", "Three outputs required.");
  }

  // Use localhost IP address
  const char *ip_address = "127.0.0.1";

  // Create a UDP socket
  SOCKET sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd == INVALID_SOCKET) {
    mexErrMsgIdAndTxt("osc:open:socket", "Error opening socket.");
  }

  // Bind the socket to an ephemeral port (0) for receiving
  sockaddr_in recv_addr;
  memset(&recv_addr, 0, sizeof(recv_addr));
  recv_addr.sin_family = AF_INET;
  recv_addr.sin_port = htons(0); // Use ephemeral port
  recv_addr.sin_addr.s_addr = INADDR_ANY;
  if (bind(sockfd, (struct sockaddr *)&recv_addr, sizeof(recv_addr)) ==
      SOCKET_ERROR) {
    mexErrMsgIdAndTxt("osc:open:bind", "Error binding socket.");
  }

  // Get the assigned port number for receiving
  int recv_addr_len = sizeof(recv_addr);
  if (getsockname(sockfd, (struct sockaddr *)&recv_addr, &recv_addr_len) ==
      SOCKET_ERROR) {
    mexErrMsgIdAndTxt("osc:open:getsockname", "Error getting socket name.");
  }
  int osc_recv_port = ntohs(recv_addr.sin_port);

  // Set up the server address structure (for sending)
  sockaddr_in send_addr;
  memset(&send_addr, 0, sizeof(send_addr));
  send_addr.sin_family = AF_INET;
  send_addr.sin_port =
      htons(osc_recv_port + 1); // Use recv_port + 1 for sending
  send_addr.sin_addr.s_addr = inet_pton(AF_INET, ip_address,
                                        &(send_addr.sin_addr)); // Use inet_pton

  // Output the socket file descriptor, send address, and receive port
  plhs[0] = mxCreateDoubleScalar((int)sockfd);
  plhs[1] = mxCreateNumericMatrix(1, sizeof(send_addr), mxUINT8_CLASS, mxREAL);
  memcpy(mxGetData(plhs[1]), &send_addr, sizeof(send_addr));
  plhs[2] = mxCreateDoubleScalar(osc_recv_port);
}

// Function to send an OSC message (updated to use send_addr and corrected
// lo_message_length)
void osc_send_message(int nlhs, mxArray *plhs[], int nrhs,
                      const mxArray *prhs[]) {
  // Check for proper number of arguments
  if (nrhs < 4) {
    mexErrMsgIdAndTxt("osc:send_message:nrhs",
                     "At least four inputs required.");
  }
  if (nlhs != 0) {
    mexErrMsgIdAndTxt("osc:send_message:nlhs", "No output required.");
  }

  // Get input arguments (socket file descriptor, send address, OSC address,
  // type tags, and data)
  int sockfd = (int)mxGetScalar(prhs[0]);
  sockaddr_in *send_addr = (sockaddr_in *)mxGetData(prhs[1]);
  char *address = mxArrayToString(prhs[2]);
  char *type_tags = mxArrayToString(prhs[3]);

  // Create an OSC message
  lo_message msg = lo_message_new();

  // Pack data into the message based on type tags
  int data_index = 4;
  for (int i = 0; type_tags[i] != '\0'; i++) {
    switch (type_tags[i]) {
    case 'i': {
      int value = (int)mxGetScalar(prhs[data_index++]);
      lo_message_add(msg, "i", value);
      break;
    }
    case 'f': {
      float value = (float)mxGetScalar(prhs[data_index++]);
      lo_message_add(msg, "f", value);
      break;
    }
    // Add more cases for other data types as needed (s, b, etc.)
    default:
      mexErrMsgIdAndTxt("osc:send_message:type_tag",
                       "Unsupported type tag.");
    }
  }

  // Send the OSC message using sendto()
  int result =
      sendto(sockfd, lo_message_serialise(msg, lo_message_length(msg, 0)),
             lo_message_length(msg, 0), 0, (struct sockaddr *)send_addr,
             sizeof(*send_addr));
  if (result == SOCKET_ERROR) {
    mexErrMsgIdAndTxt("osc:send_message:send", "Error sending OSC message.");
  }

  // Clean up
  lo_message_free(msg);
  mxFree(address);
  mxFree(type_tags);
}

// Function to read a parameter value (experimental)
void osc_read_param(int nlhs, mxArray *plhs[], int nrhs,
                    const mxArray *prhs[]) {
  // Check for proper number of arguments
  if (nrhs != 2) {
    mexErrMsgIdAndTxt("osc:read_param:nrhs", "Two inputs required.");
  }
  if (nlhs != 1) {
    mexErrMsgIdAndTxt("osc:read_param:nlhs", "One output required.");
  }

  // Get input arguments (socket file descriptor and OSC address)
  int sockfd = (int)mxGetScalar(prhs[0]);
  char *address = mxArrayToString(prhs[1]);

  // Send an OSC message to request the parameter value (experimental)
  lo_message msg = lo_message_new();
  int result =
      lo_send_from_sockfd(sockfd, address, msg); // Use lo_send_from_sockfd
  lo_message_free(msg);
  if (result == -1) {
    mexErrMsgIdAndTxt("osc:read_param:send", "Error sending OSC message.");
  }

  // Receive the OSC response (experimental)
  char buffer[OSC_BUFFER_SIZE];
  struct sockaddr_in from;
  socklen_t fromlen = sizeof(from);
  result = lo_recv_from(sockfd, buffer, OSC_BUFFER_SIZE,
                        (struct sockaddr *)&from, &fromlen);
  if (result < 0) {
    mexErrMsgIdAndTxt("osc:read_param:recv", "Error receiving OSC message.");
  }

  // Parse the OSC response and extract the value (experimental)
  lo_message rcv_msg = lo_message_deserialise(buffer, result, &from);
  if (rcv_msg == NULL) {
    mexErrMsgIdAndTxt("osc:read_param:deserialize",
                     "Error deserializing OSC message.");
  }

  float value = 0.0f;
  lo_arg **argv = lo_message_get_argv(rcv_msg);
  int argc = lo_message_get_argc(rcv_msg);
  if (argc > 0) {
    value = (float)argv[0]->f;
  }

  // Output the value
  plhs[0] = mxCreateDoubleScalar(value);

  // Clean up
  lo_message_free(rcv_msg);
  mxFree(address);
}

// Function to close the OSC connection
void osc_close(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[]) {
  // Check for proper number of arguments
  if (nrhs != 1) {
    mexErrMsgIdAndTxt("osc:close:nrhs", "One input required.");
  }
  if (nlhs != 0) {
    mexErrMsgIdAndTxt("osc:close:nlhs", "No output required.");
  }

  // Get input argument (socket file descriptor)
  int sockfd = (int)mxGetScalar(prhs[0]);

  // Close the socket
  closesocket(sockfd); // Use closesocket for Windows
}

// Gateway function for MEX
void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[]) {
  // Get the command string
  if (nrhs < 1) {
    mexErrMsgIdAndTxt("osc:nrhs", "At least one input required.");
  }
  char *cmd = mxArrayToString(prhs[0]);

  // Call the appropriate function based on the command
  if (strcmp(cmd, "open") == 0) {
    osc_open(nlhs, plhs, nrhs - 1, prhs + 1);
  } else if (strcmp(cmd, "send_message") == 0) {
    osc_send_message(nlhs, plhs, nrhs - 1, prhs + 1);
  } else if (strcmp(cmd, "read_param") == 0) {
    osc_read_param(nlhs, plhs, nrhs - 1, prhs + 1);
  } else if (strcmp(cmd, "close") == 0) {
    osc_close(nlhs, plhs, nrhs - 1, prhs + 1);
  } else {
    mexErrMsgIdAndTxt("osc:cmd", "Unknown command.");
  }

  mxFree(cmd);
}