import socket
import sys

def send_command(command_line, host="127.0.0.1", port=12345, timeout=5, expect_response=False):
    """
    Connects to the Z80 Explorer command server and sends a single command line.

    Args:
        command_line (str): The command string to send.
        host (str): The hostname or IP address of the server.
        port (int): The port number of the server.
        timeout (int): Socket timeout in seconds.
        expect_response (bool): If True, waits for a single line response.

    Returns:
        str or None: The response from the server if expect_response is True, 
                     None otherwise or if an error occurs.
    """
    response = None
    try:
        # Ensure the command ends with a newline, as the server reads line by line
        if not command_line.endswith('\n'):
            command_line += '\n'

        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.settimeout(timeout)
            s.connect((host, port))
            s.sendall(command_line.encode('utf-8'))
            print(f"Sent to {host}:{port}: {command_line.strip()}")

            if expect_response:
                # Wait for a simple acknowledgment or response
                # This is a very basic response handling, assumes single line.
                buffer = bytearray()
                while True:
                    try:
                        chunk = s.recv(1024)
                        if not chunk:
                            # Connection closed by server before newline or after full message
                            break 
                        buffer.extend(chunk)
                        if b'\n' in buffer: # Check if a newline is received
                            break
                    except socket.timeout:
                        print("Timeout waiting for response.")
                        break # Exit if timeout occurs
                
                if buffer:
                    response = buffer.decode('utf-8').strip()
                    print(f"Received: {response}")
                else:
                    print("No response received or connection closed.")
                    
    except socket.timeout:
        print(f"Error: Connection to {host}:{port} timed out.")
    except socket.error as e:
        print(f"Error connecting or sending data to {host}:{port}: {e}")
    except Exception as e:
        print(f"An unexpected error occurred: {e}")
    return response

if __name__ == "__main__":
    server_host = "127.0.0.1"
    server_port = 12345

    print("Python Command Client")
    print("---------------------")
    print(f"Attempting to send commands to Qt server at {server_host}:{server_port}")
    print("Type a command and press Enter to send. Type 'exit' or 'quit' to close this client.")

    while True:
        try:
            user_input = input("Cmd> ")
            if user_input.lower() in ["exit", "quit"]:
                break
            if not user_input:
                continue

            # Send command and expect a response (as implemented in the Qt example)
            res = send_command(user_input, server_host, server_port, expect_response=True)
            # If you don't expect a response, set expect_response=False
            # send_command(user_input, server_host, server_port)

        except KeyboardInterrupt:
            print("\nExiting client.")
            break
        except Exception as e:
            print(f"Client loop error: {e}")
            break
