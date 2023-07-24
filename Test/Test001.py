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
    print ('\nCommand sent: "{}"'.format(cmd))
    sock.sendall(bytes(cmd, 'utf8'))
    sock.sendall(b'\n')

# receive reply from target
def getReply():
    reply = b''
    done = False
    while not done:
        c = sock.recv(1024)
        if not c:
            print ('Connection dropped!')
            done = True
            continue
        reply += c
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
    
# skip over first prompt
getReply()

# command list
cmdList = [
    'help',
    'list',
    'version',
    '{"one":1,"two":2,"three":3,"four":4,"five":5,"six":6,"seven":7,"eight":8,"nine":9,"ten":10}',
    'count',
    'json',
    'sn?',
    'runType?'
    ]

# send command list to target
# TODO check response time, to see if commands are being throttled
for cmd in cmdList:
    sendCmd(cmd)
    reply = getReply()
    if not len(reply): continue
    print ('\nReply received: {}'.format(reply))
    jsonText = findJson(reply)
    if not len(jsonText): continue
    print ('\nReply decoded as Json: {}'.format(json.dumps(jsonText, indent = 2, sort_keys = True)))

# clean up and exit
sendCmd('done')
sock.close()
quit()
