# Simple tasks from Computer networks

On a certain computer, the command traceroute kampus.uw.edu.pl was executed and the following result was obtained (response times were omitted):

traceroute to kampus.uw.edu.pl (193.0.115.236), 30 hops max, 60 byte packets
 1  _gateway (192.168.2.1)
 2  * * *
 3  * * *
 4  * * *
 5  wa1.equinix.com (195.182.218.53)
 6  palo.uw.edu.pl (193.0.103.16)
 7  kampus.uw.edu.pl (193.0.115.236)

The command traceroute kampus.uw.edu.pl was also executed on the computer students.mimuw.edu.pl with the result:

traceroute kampus.uw.edu.pl
traceroute to kampus.uw.edu.pl (193.0.115.236), 30 hops max, 60 byte packets
 1  c96.mimuw.edu.pl (193.0.96.31)
 2  palo.uw.edu.pl (193.0.103.37)
 3  kampus.uw.edu.pl (193.0.115.236)

On students.mimuw.edu.pl, the command traceroute ns3.uw.edu.pl was also executed and resulted in:

traceroute to ns3.uw.edu.pl (193.0.94.133), 30 hops max, 60 byte packets
 1  c96.mimuw.edu.pl (193.0.96.31)
 2  palo.uw.edu.pl (193.0.103.37)
 3  ns3.uw.edu.pl (193.0.94.133)

Let's adopt the following notation:

    wa1 – wa1-ix.equinix.com
    palo – palo.uw.edu.pl
    kampus – kampus.uw.edu.pl
    ns3 – ns3.uw.edu.pl
    students – students.mimuw.edu.pl
    c96 – c96.mimuw.edu.pl

a) Draw a logical connection diagram of the servers kampus, ns3, students and routers wa1, palo, and c96. Assign interface names and their IP addresses along with subnet masks. Where possible, assign a /24 mask. Fill in the missing information in such a way that it is consistent with the traceroute command results presented above and the connection diagram shown.

b) Write the result of the traceroute command when tracing the route from the computer ns3.uw.edu.pl to the router c96.mimuw.edu.pl, assuming that packets pass through the routers mentioned in the task.

c) Write a fragment of the routing table of the palo router enabling the communication presented above. All entries in the routing table should be explicit and expressed numerically. Assume that the default route leads to router 193.0.103.1.

d) Provide a fragment of the contents of the uw.edu.pl zone file containing all name-to-address mappings appearing in the task description and in the solution. Assume that ns3 is the DNS server for this zone. Include the SOA record.

As a solution, upload a PDF file to Moodle. The solution should contain the author's first name, last name, and student index number.
