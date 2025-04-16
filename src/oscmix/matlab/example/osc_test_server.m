% creating a server -- just specify what port to listen on

port_num_in = 7001;
port_num_out = 9001;
s = osc_new_server(port_num_out)

for t = 1:10

  % --------------------------------

  % when recieving, specify a timeout in seconds
  % a timeout of zero can be used for non-blocking operation
  % m will always be a cell array containing zero or more messages
  %
  % ... osc_server_recv() always flushes the buffer completely,
  % it returns all the messages that are available for processing 
  % in order from oldest to newest.  the buffer can hold up to about 500 
  % messages before it begins to discard old messages (FIFO)
  %

  m = osc_recv(s, 10.0)

  % check to see if anything is there...
  if ~isempty(m)

    % the address of the first message..
    m{1}.path

    % and its data part
    m{1}.data
  
    % the last message... etc.
    m{length(m)}
  end

end

s = osc_free_server(s);

