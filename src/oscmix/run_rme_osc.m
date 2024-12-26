% Open the OSC connection
rme_ip = '192.168.1.100'; % Replace with the actual IP address of your RME device
rme_port = 10000;
sockfd = osc_rme('open', rme_ip, rme_port);

% Read the current gain of the first microphone channel (experimental)
gain = osc_rme('read_param', sockfd, '/headamp_1/gain');

% Set the gain of the first microphone to +6 dB (assuming a linear mapping)
db_value = 6.0;
float_value = (db_value + 70.0) / 70.0; %// Adjust mapping as needed
osc_rme('send_message', sockfd, '/headamp_1/gain', 'f', float_value);

% Close the OSC connection
osc_rme('close', sockfd);