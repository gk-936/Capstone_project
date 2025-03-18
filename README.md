
*Secure Chat Application in C (with RSA Encryption & AI Assistant)*  

  ##*Overview*  
This project implements a *secure chat application* using the *C programming language, featuring **end-to-end RSA encryption* and an *AI-powered assistant. It follows a **client-server model* where users can securely communicate and query an AI assistant for intelligent responses.  

   ##*Objectives*  
-  Develop a *real-time* messaging system using the *client-server model* in C.  
-  Implement *RSA encryption* to ensure secure transmission.  
-  Integrate an *AI assistant* that provides intelligent responses via the *Ollama API (Mistral model)*.  
-  Achieve *efficient and optimized communication* between users.  

   ##*Features*  

 🔹 *Client-Server Communication*  
- Supports *real-time messaging* between multiple clients.  
- Uses a *dedicated server* to manage direct routing.  
- Processes *user input and responses* dynamically.  

 🔹 *End-to-End Encryption (RSA)*  
-  *Key Generation*: Generates public-private key pairs.  
-  *Encryption: Messages are encrypted using the **public key*.  
-  *Decryption: The receiver decrypts messages with their **private key*.  

 🔹 *AI Chat Assistant Integration*  
-  *Command*: Users can query an AI assistant using \ask <question>.  
-  *Process*:  
  1. Client sends a query → Server forwards to *Ollama API (Mistral model)*.  
  2. API processes the request and returns a *JSON response*.  
  3. Server extracts the *AI-generated answer* and sends it back to the client.  
-  *Tech Stack: Uses **cURL for API requests* and *JSON parsing for responses*.  



  ##*How It Works*  

 *Start the Server*  
bash
./server

 *Launch the Client*  
bash
./client

#enter the server IP address after running client
 *Chat Securely*  
- Type messages to send them securely.  
- Use \ask <question> to get AI-generated responses.  



   ##*Conclusion*  
This project establishes a *foundation for secure and intelligent communication* using *C programming. The integration of **RSA encryption and AI assistance* ensures both *privacy* and *smart interactions*.  




