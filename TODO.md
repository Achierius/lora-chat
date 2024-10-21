From 2024-10-18:
    - Implement a dual advertise-seek mode
        - Add randomization to the dual advertise-seek mode to prevent overlap
    - Adjust the lora library to allow for different configuration settings (perhaps just meshtastic presets?)
    - Do ToA computation inside libsx1276 (just hardcode functions per preset ^ ?)
    - Fix the 1-2 blank messages received at the start of each session
From 2024-08-28:
    - Hide library internals behind opaque APIs
From 2024-08-17:
    - Move time constants out of ProtocolAgent
    - Make new facade class on top of ProtocolAgent which can handle
        - Threading & message queueing
        - Hooking into the radio
        - User interface
    - Write wrapper tools which use that class to provide a shell interface
    - Plumb timeouts into the lora radio interface
    - Add a way for sessions to gracefully terminate the connection & flush their message buffer
    - Add resynchronizing packets to sessions if necessary
        - Currently we get somewhere between 30 and 60 minutes before desync happens
