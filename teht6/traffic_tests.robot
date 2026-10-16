*** Settings ***
Library         TrafficSerial.TrafficSerial
Suite Setup     Open Serial Port    ${PORT}    ${BAUDRATE}
Suite Teardown  Close Serial Port
Test Setup      Reset Failure Counter

*** Variables ***
${PORT}     COM5
${BAUDRATE}    115200

*** Test Cases ***
Time Command Returns Seconds
    ${response}=    Send Command    000120
    Should Be Equal    ${response}    80X

Reject Incorrect Time Length
    ${response}=    Send Command    1234
    Should Be Equal    ${response}    -2X

Reject Non-Numeric Time
    ${response}=    Send Command    00A000
    Should Be Equal    ${response}    -3X

Reject Zero Time
    ${response}=    Send Command    000000
    Should Be Equal    ${response}    -4X

Reject Out Of Range Time
    ${response}=    Send Command    240000
    Should Be Equal    ${response}    -1X

Accept Valid Traffic Sequence
    ${response}=    Send Command    RYG
    Should Be Equal    ${response}    1X

Reject Invalid Traffic Sequence
    ${response}=    Send Command    RjG
    Should Be Equal    ${response}    -11X

Debug Command Returns Debug Value
    ${response}=    Send Command    D
    Should Be Equal    ${response}    1013msX

Accept Timed Sequence Command
    ${response}=    Send Command    R,100,Y,500
    Should Be Equal    ${response}    2X

Reject Zero Timed Sequence Delay
    ${response}=    Send Command    R,0
    Should Be Equal    ${response}    -12X

*** Keywords ***
Reset Failure Counter
    ${response}=    Send Command    000120
    Should Be Equal    ${response}    80X
