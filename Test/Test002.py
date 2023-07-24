import sys, socket, os, time, json

# socket setup
host = '172.29.33.16'
# host = '192.168.1.118'
port = 10240
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect((host, port))
if not sock:
    print ("Failed to open socket {0}:{1}".format(host, port))
    quit()

# send command to target
def sendCmd(cmd):
    cmd = cmd.rstrip()
    print ('Command sent: "{}"'.format(cmd))
    sock.sendall(bytes(cmd, 'utf8'))
    sock.sendall(b'\n')

# receive reply from target
def getReply():
    reply = b''
    done = False
    while not done:
        a = sock.recv(1024)
        if not a:
            print ('Connection dropped!')
            done = True
            continue
        reply += a
        replyLen = len(reply) - 7
        if replyLen < 0: replyLen = 0
        if reply.find(b"10240: ", replyLen) > -1:
            done = True
    return str(reply[0:replyLen], 'utf8')

# find JSON text in reply string
def findJson(reply):
    jsonText = ""
    arrayFound = False
    dictionaryFound = False
    openBrace = reply.find('{')
    closeBrace = reply.rfind('}')
    openBracket = reply.find('[')
    closeBracket = reply.rfind(']')
    if (openBrace > -1) and (closeBrace > openBrace): dictionaryFound = True
    if (openBracket > -1) and (closeBracket > openBracket): arrayFound = True
    if dictionaryFound and arrayFound:
        if openBrace < openBracket: arrayFound = False
        else: dictionaryFound = False
    if arrayFound: jsonText = reply[openBracket:closeBracket+1]
    elif dictionaryFound: jsonText = reply[openBrace:closeBrace+1]
    jsonText = jsonText.replace('\r\n','\n')
    return jsonText

# send Json file to target, suppressing newlines until the end
def sendFile(file):
    while True:
        char = file.read(1)
        if char:
            if (char == '\r'): continue
            if (char == '\n'): continue
            if (char == '\t'): continue
            sock.sendall(bytes(char, 'utf8'))
        else:
            sock.sendall(b'\n')
            return
    
# skip over first prompt
getReply()

# command list
cmdList = [
    'help',
    'list',
    'version',
    'count',
    'json',
    '{"one":1,"two":2,"three":3,"four":4,"five":5,"six":6,"seven":7,"eight":8,"nine":9,"ten":10}',
    'count',
    'json'
    ]

# send and confirm Json files
fileList = [
    'Input.json',
    'Test.json',
    'Ref.json'
    ]

for n in range(1, 10):
    for inFileName in fileList:
        with open(inFileName, 'r', encoding="utf8") as inFile:
            sendFile(inFile)
            getReply()
            for line in cmdList:
                sendCmd(line)
                reply = getReply()
                if len(reply):              
                    jsonText = findJson(reply)
                    if len(jsonText):
                        print ('Json received: \n{}'.format(json.loads(jsonText)))
                    else:
                        print ('Reply received: \n{}'.format(reply))
            
# clean up and exit
sendCmd('done')
sock.close()
quit()
