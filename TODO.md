From 2024-08-17:
    - Move time constants out of ProtocolAgent
    - Make new facade class on top of ProtocolAgent which can handle
        - Threading & message queueing
        - Hooking into the radio
        - User interface
    - Write wrapper tools which use that class to provide a shell interface
    - Refactor the packet layout to include the destination ID
    - Plumb timeouts into the lora radio interface
    - Let sessions timeout after a few nack attempts
    - Add a way for sessions to gracefully terminate the connection & flush their message buffer
    - Add resynchronizing packets to sessions if necessary
    - Add randomization to the dual advertise-seek mode to prevent overlap
    - Allow for different packet types to have different lengths so advertising packets can be shorter
From 2024-08-28:
    - Hide library internals behind opaque APIs
