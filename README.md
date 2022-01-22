# nshwsl
WSL network helper tool

(Re)creates the WSL network with a specific subnet `192.168.222.0/24` and adds an `alias 192.168.222.222` for the assigned range.  
In case the IP is already set, the error can be ignored.  
This works on Windows 10+ but requires sudo permissions for the `ip addr` command.  
In Windows 11 /etc/wsl.conf supports a command in the [boot] section executed as root to set the IP.  
For now use the same approach also for Win 11.

Supports to specify a class C IP and calculates the network and gateway addresses accordingly.

You can specify an explicit IP address. In this case the class C subnet is automatically calculated.

Example:

```
nshwsl.exe 192.168.222.222

nshwsl: WSL Network setup helper tool
Existing WSL Network deleted
WSL Network created

WSL Network setup done
----------------------------------------
Address: [192.168.222.222]
Network: [192.168.222.0]
Gateway: [192.168.222.1]
```

## References and thanks for inspiration:

https://github.com/microsoft/WSL/discussions/7395
https://github.com/skorhone/wsl2-custom-network

## Useful commands:

### For testing: Powershell Get current settings :

```
Get-HnsNetwork | Where-Object { $_.Name -Eq "WSL" }  ( | Remove-HNSNetwork )
Get-HNSEndpoint  ( | Remove-HNSEndpoint )
```

### Get WSL container IP:

```
wsl hostname -I
```

### Set addtional IP

```
wsl sudo ip a add 192.168.222.222/24 dev eth0
```

### List running VMs

```
hcsdiag.exe list
```
