% osc_rme_example.m

% IP address of the RME device (localhost)
ip_address = '127.0.0.1';

% OSC send and receive ports
osc_port_out = 9001;  % Output port
osc_port_in = 7001;   % Input port

% Create an OSC sender
osc_sender = oscmex('create', ip_address, osc_port_out);

% Create an OSC receiver
osc_receiver = oscmex('create', osc_port_in);

% Define a callback function to handle received messages 
function msg_struct = osc_callback(address, type_tags, data, source)
  % Create a struct to store the message information
  msg_struct.address = address;
  msg_struct.type_tags = type_tags;
  msg_struct.data = data;
  msg_struct.source = source; 
end

% Set the callback function 
oscmex('set_callback', osc_receiver, @osc_callback);

% Start listening for OSC messages
oscmex('listen', osc_receiver);

% Read the current gain of the first microphone channel (experimental)
% This assumes the RME device responds with the current value when 
% an OSC message is sent to the parameter address without any arguments.
try
    oscmex('send', osc_sender, '/headamp_1/gain');
    pause(0.1); % Wait for a short time to receive the response
    [messages, sources] = oscmex('received', osc_receiver);
    if ~isempty(messages)
        % Assuming the response is a single float value
        gain = messages{1}{3}; 
        disp(['Current gain: ', num2str(gain)]);
    else
        disp('No response received.');
    end
catch
    disp('Error reading gain.');
end

% Set the gain of the first microphone to +6 dB
db_value = 6.0;
float_value = (db_value + 70.0) / 70.0; % Adjust mapping as needed
oscmex('send', osc_sender, '/headamp_1/gain', 'f', float_value);

% ... other OSC commands ...

% Retrieve received messages (if any)
[messages, sources] = oscmex('received', osc_receiver);

% Process the received messages
if ~isempty(messages)
    for i = 1:length(messages)
        msg_struct = messages{i}; 
        % Access message fields
        fprintf('Received OSC message:\n');
        fprintf('  Address: %s\n', msg_struct.address);
        fprintf('  Type tags: %s\n', msg_struct.type_tags);
        fprintf('  Data: ');
        disp(msg_struct.data);
        fprintf('  Source: %s\n', msg_struct.source);
    end
end


% Stop listening for OSC messages
oscmex('listen', osc_receiver, 'off');

% Delete the OSC sender and receiver
oscmex('delete', osc_sender);
oscmex('delete', osc_receiver);