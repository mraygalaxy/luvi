#!/usr/bin/env python

from DocXMLRPCServer import DocXMLRPCServer
import os
import subprocess

# Create server
server = DocXMLRPCServer(("0.0.0.0", 8000))
server.register_introspection_functions()

def perform(cmd) :
    print "performing: " + cmd
    output = ""
    p = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    for line in p.stdout.readlines():
        output += line
    retval = p.wait() 

    print "returning: *" + output + "*"
    return {"code": retval, "result" : output}

class LuviRelay :
    def kill(self, what) :
        cmd="/tmp/luvi-web/scripts/kill.sh"
        return perform(cmd) 

    def slave(self, master) :
        cmd="screen -d -m -S luvislave bash -c '/tmp/luvi-web/scripts/slave.sh " + master + "'"
        return perform(cmd) 

    def master(self, filename, src) :
        cmd="screen -d -m -S luvimaster bash -c '/tmp/luvi-web/scripts/master.sh " + filename + " " + src + " 2>&1 >> /tmp/luvi-web/luvi_master.log'"
        return perform(cmd) 

    def status(self) :
        return perform("tail -10 /tmp/luvi-web/luvi_master.log")

print "Registered luvi functions"
lr = LuviRelay()
server.register_instance(lr)
print "Serving..."
server.serve_forever()
