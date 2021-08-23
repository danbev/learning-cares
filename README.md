### Learning c-ares 
This repo exists to keep notes and runnable examples to help me learn and also
troubleshoot c-ares.

c-ares was originally a fork of ares by libcurl and named c-ares (for curl and
not for the C language which what I initially thought). It is an asynchronous
DNS resolver library.

One thing that might be obvious to some but was not to me is that a DNS resolver
can be used for more than looking up a host address, it can also be used to
get mail information and there are a number of other request/quieries possible.

### Building c-ares
We want to build with debug symbols so that we can step through the code:
```console
$ mkdir build
$ cmake -DCMAKE_BUILD_TYPE=Debug -DCARES_BUILD_TESTS=True ..
$ make -j8
$ make -j8 test
```

### Linking with c-ares
```console
$ make out/first
$ env LD_LIBRARY_PATH=/home/danielbevenius/work/network/c-ares/build/lib64/ ldd out/first
	linux-vdso.so.1 (0x00007fff717f7000)
	libcares.so.2 => /home/danielbevenius/work/network/c-ares/build/lib64/libcares.so.2 (0x00007f07b91cb000)
	libc.so.6 => /usr/lib64/libc.so.6 (0x00007f07b8fdd000)
	/lib64/ld-linux-x86-64.so.2 (0x00007f07b91eb000)
```

### Walk through
```console
$ env LD_LIBRARY_PATH=/home/danielbevenius/work/network/c-ares/build/lib64/ lldb -- out/first
$ br s -n main
```
The first thing that needs to be done it to initialize c-ares which is done
using ares_library_init which can be found in 

```console
(lldb) expr *host
(hostent) $1 = {
  h_name = 0x0000000000417860 "google.com"
  h_aliases = 0x0000000000417840
  h_addrtype = 2
  h_length = 4
  h_addr_list = 0x0000000000417e00
}
```

### Channel
```c
typedef struct ares_channeldata *ares_channel;
```
And the definition can be found in `c-ares/src/lib/ares_private.h`:
```c
struct ares_channeldata {
  ...
  ares_sock_state_cb sock_state_cb;
  ares_sock_create_callback sock_create_cb;
  ares_sock_config_callback sock_config_cb;
};
```

### Project configuration
Generate, or regenerate the ctags file:
```console
$ ctags -R . ../c-ares/
```

### DNS Resolver lookup
This is simplified quite a bit I'm sure but basically we have something like
this:
```

+--------+       +--------------+ ------------> +-------------+
| client |------>| DNS Resolver |               | Root Server |
+--------+       |              | <------------ +-------------+
                 |              | ------------> +-------------+
                 |              |               | TLD Server  | (Top Level Domain Server)
                 |              | <------------ +-------------+
                 |              | ------------> +-------------------------+
                 |              |               | Authorative Name Server |
                 +--------------+ <-----------  +-------------------------+
``` 
So a client sends a hostname to the DNS Resolver which if it does not have an
ip cached for that hostname will send it to the Root Server. The Root Server
will return a Top Level Domain Server, which the DNS Resolver will call and
get back an Authorative Name Server which it will then call to get the ip
address. This ip address will then be cached and the ip returned to the client.

### DNS Record Types

#### Address Mapping record (A Record)
Is a mapping of a hostname to an IPv4 address

#### IP Version 6 Address record (AAAA Record)
Is similar to a A Record but for Ipv6.
Why the four As? 

#### Pointer Record (PTR Record)
Is the opposite of an A Record, so it maps an ip address to a hostname.
These are used for reverse DNS lookups, where a normal lookup uses a hostname
this type will use an ip address and then looks up the hostname.
The ip addresses are stored with a `.in-addr.arpa` suffix, for example:
```
255.2.0.192.in-addr.arpa
```
`.arpa` is a top-level domain which is used for managing network infrastructure.
ARPA = Advanced Research Project Agency.
And in-addr is the the namespace for reverse DNS lookups in IPv4.

For IPv6 there is a separate namespace `.ip6.arpa`.

One usecase for reverse DNS lookup is logging where normally logs contain ip
addresses and one might want to convert them into more human readable hostnames.

#### Canonical Name record (CNAME Record)
Is an alias for a host name to another hostname. So lets say we have an A Record
which is mapping on hostname1 to an IPv4 address, then we could create a CNAME
record that points to hostname1, but with a different name, say hostname2.

#### Text Record (TXT Record)
Usually carries machine readable data and can be used to prevent email spamming
by various entries/format for these TXT records. Sender Policy Framework (SPR)
lists all the servers that are authorized to send email messages from a domain
for example.

#### Name Server record (NS Record)
These records specify which DNS server contains the ip of the requested hostname
A name server stores DNS records for a domain which includes A Records, MX
Records, or CNAME Records).

#### Service Location Record (SRV Record)
Is a record that specifies a host/port for specific services (think servies in
linux/unix in /etc/services). Notice that these records also contain a port.
The format looks like this:
```text
_service._proto.name. TTL class type of record priority weight port target
```
And an example:
```
_xmpp._tcp.example.com. 86400 IN SRV 10 5 5223 server.example.com.
```
The `target`, in the example above `server.example.com` must point to a A
Record/AAAA Record.

#### Certification Authority Authorization (CAA record)
These records are used to specify which Certificate Authorities (CA) are allowed
to issue certificates for a domain.


#### Fully Qualified Domain Name (FQDN)
This is a name ends with a `.` it will be considered fully qualified and no
local search will be done.

#### Hostname vs Domain name
The POSIX Standard defines the data format of these host names as a
null-terminated C-String containing a "hostname" or "nodename", which are
typically expected by developers and defined by [RFC952](https://datatracker.ietf.org/doc/html/rfc952)
and RFC1123 to only contain alphanumeric characters (a-z,A-Z,0-9), hyphens ("-")
and periods (".") to split labels. This creates a mismatch of allowed characters
between "hostnames" and "domain names" as defined by the DNS standard which
defines "domain names" as a series of "text labels" which are textually
represented by concatenating all "text labels" and joining them together with
period signs.

However, "text labels" can contain any octet value, even zero-bytes ("\x00") and
period signs (".") and recursive DNS resolvers are required by the DNS standard
to support any of these characters in DNS records, thus not implementing any
sanitiy checks on domain names.


### DNS Tunneling
Is an attack where DNS requests are routed to the attackers server providing
them with a covert command and control channel (C2) and a way to exfiltrate
data (extracting/sending out data I think).

An attacher could register a domain and set up a DNS server as the authorative
server for this domain. A program on the compromised client sends DNS queries
to a subdomain of the registered domain. The attacker can configure the DNS
server to route request to his machine for this subdomain.
The DNS server can now send responses that are malicious which can include data
and the nice thing here is that this will be allowed to be sent across the
network security perimiters like
firewalls.



