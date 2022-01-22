
/*

    Copyright Nash!Com, Daniel Nashed 2019, 2020 - APACHE 2.0 see LICENSE
    Author: Daniel Nashed (Nash!Com)

    Licensed to the Apache Software Foundation (ASF) under one
    or more contributor license agreements.  See the NOTICE file
    distributed with this work for additional information
    regarding copyright ownership.  The ASF licenses this file
    to you under the Apache License, Version 2.0 (the
    "License"); you may not use this file except in compliance
    with the License.  You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing,
    software distributed under the License is distributed on an
    "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
    KIND, either express or implied.  See the License for the
    specific language governing permissions and limitations
    under the License.

    --------------------------------------------------------------------------------

    (Re)creates the WSL network with a specific subnet 192.168.222.0/24 and adds an alias 192.168.222.222 for the assigned range.
    In case the IP is already set, the error can be ignored.
    This works on Windows 10+ but requires sudo permissions for the "ip addr " command.
    In Windows 11 /etc/wsl.conf supports a command in the [boot] section executed as root to set the IP.
    For now use the same approach also for Win 11.

    Supports to specify a class C IP and calculates the network and gateway addresses accordingly.

    References and thanks for inspiration:
        https://github.com/microsoft/WSL/discussions/7395
        https://github.com/skorhone/wsl2-custom-network

    For testing: Powershell Get current settings :
        Get-HnsNetwork | Where-Object { $_.Name -Eq "WSL" }  ( | Remove-HNSNetwork )
        Get-HNSEndpoint  ( | Remove-HNSEndpoint )

    Useful commands:

    Get WSL container IP:
        wsl hostname -I

    Set addtional IP
        wsl sudo ip a add 192.168.222.222/24 dev eth0

    List running VMs
        hcsdiag.exe list
*/

#include <stdio.h>
#include <computenetwork.h>
#include <computecore.h>
#include <Objbase.h>
#include <tlhelp32.h>

#pragma comment(lib, "computenetwork") /* Requires 64 bit compiler */
#pragma comment(lib, "computecore")
#pragma comment(lib, "Ole32.lib")

#define WSL_NETWORK_GUID   L"{B95D0C5E-57D4-412B-B571-18A81A16E005}" /* Hardcoded network GUID used by WSL */
#define DEFAULT_IP_ADDRESS "192.168.222.222"

DWORD GetProcessIdByName (char *pszProcessName)
{
    DWORD          pid       = 0;
    HANDLE         hSnapshot = INVALID_HANDLE_VALUE;
    PROCESSENTRY32 entry     = {0};

    hSnapshot = CreateToolhelp32Snapshot (TH32CS_SNAPPROCESS, 0);

    if (INVALID_HANDLE_VALUE == hSnapshot)
        goto Done;

    entry.dwSize = sizeof (entry);

    if (0 == Process32First (hSnapshot, &entry))
        goto Done;

    do
    {
      if (0 == strcmp (entry.szExeFile, pszProcessName))
      {
        pid = entry.th32ProcessID;
        break;
      }

    } while (Process32Next (hSnapshot, &entry));

Done:

    if (INVALID_HANDLE_VALUE != hSnapshot)
    {
        CloseHandle (hSnapshot);
        hSnapshot = INVALID_HANDLE_VALUE;
    }

    return pid;
}

int WSL_CheckAdressAlreadySet (const char *pszIpAddress)
{
    FILE *pFile         = NULL;
    char szBuffer[4096] = {0};
    int  found          = 0;

    pFile = _popen ("wsl.exe ip addr", "r");

    if (NULL == pFile)
        goto Done;

    while (fgets (szBuffer, sizeof (szBuffer) -1, pFile))
    {
        if (strstr (szBuffer, pszIpAddress))
            found++;
    }

Done:

    if (pFile)
    {
        _pclose (pFile);
        pFile = NULL;
    }

    return found;
}


int WSL_CreateUpdateNetwork (const char *pszIpAddress)
{
    int          ret            = -1;
    int          found          = 0;
    int          a,b,c,d, n     = 0;
    size_t       nChars         = 0;
    HRESULT      hRes           = 0;
    DWORD        dwWslHostPID   = 0;
    HCN_NETWORK  pHcnNetwork    = NULL;
    HCN_ENDPOINT pHcnEndpoint   = NULL;
    PWSTR        pErrRecord     = NULL;
    PWSTR        pStrGUID       = NULL;
    GUID         EndpointGUID   = {0};
    GUID         NetworkGUID    = {0};

    char    szIpAddress[100]    = {0};
    char    szBuffer[1024]      = {0};
    wchar_t szwJsonBuffer[4096] = {0};
    wchar_t szwIpAddress[100]   = {0};
    wchar_t szwIpNetwork[100]   = {0};
    wchar_t szwIpGateway[100]   = {0};

    // Flags = EnableDnsProxy + IsolateVSwitch
    // Type = Static

    PCWSTR networkSettingsJSON = LR"(
    {
        "Name" : "WSL",
        "Flags": 9,
        "Type": "ICS",
        "IPv6": false,
        "IsolateSwitch": true,
        "MaxConcurrentEndpoints": 1,
        "Subnets" : [
            {
                "ID" : "FC437E99-2063-4433-A1FA-F4D17BD55C92",
                "ObjectType": 5,
                "AddressPrefix" : "%s/24",
                "GatewayAddress" : "%s",
                "IpSubnets" : [
                    {
                        "ID" : "4D120505-4222-4CB2-8C53-DC0F70049696",
                        "Flags": 3,
                        "IpAddressPrefix": "%s/24",
                        "ObjectType": 6
                    }
                ]
            }
        ],
        "MacPools":  [
            {
                "EndMacAddress":  "00-15-5D-52-C0-FF",
                "StartMacAddress":  "00-15-5D-52-C0-00"
            }
        ]
    })";

    PCWSTR endpointSettingsJSON = LR"(
    {
        "Name": "Ethernet",
        "IpAddress": "%s",
        "PrefixLength": 24,
        "MacAddress": "00-15-5D-52-C0-22",
        "VirtualNetwork" : "B95D0C5E-57D4-412B-B571-18A81A16E005",
        "VirtualMachine" : "188276C0-3D4C-45B8-94D5-2DABD11F4654"
    })";

    printf ("\nnshwsl: WSL Network setup helper tool\n");

    if (pszIpAddress && *pszIpAddress)
    {
        snprintf (szIpAddress,  sizeof (szIpAddress),  "%s", pszIpAddress);
        mbstowcs_s (&nChars, szwIpAddress, sizeof (szwIpAddress)/2, pszIpAddress, _TRUNCATE);
    }
    else
    {
        snprintf (szIpAddress,  sizeof (szIpAddress),  "%s", DEFAULT_IP_ADDRESS);
        mbstowcs_s (&nChars, szwIpAddress, sizeof (szwIpAddress)/2, pszIpAddress, _TRUNCATE);
    }

    n = sscanf (szIpAddress, "%d.%d.%d.%d", &a, &b, &c, &d);

    if (n < 4)
    {
        printf ("Invalid IP address specified!\n");
        goto Done;
    }

    /* Check first if WSL is already running */
    dwWslHostPID = GetProcessIdByName ("wslhost.exe");

    _snwprintf (szwIpNetwork,  sizeof (szwIpNetwork),  L"%u.%u.%u.%u", a, b, c, 0);
    _snwprintf (szwIpGateway,  sizeof (szwIpGateway),  L"%u.%u.%u.%u", a, b, c, 1);
    _snwprintf (szwJsonBuffer, sizeof (szwJsonBuffer), networkSettingsJSON, szwIpNetwork, szwIpGateway, szwIpNetwork);

    /* Check if IP address is already set in running WSL container */
    if (dwWslHostPID)
    {
        found = WSL_CheckAdressAlreadySet (szIpAddress);
        if (found)
        {
            printf ("\nWSL address already set!\n\n");
            goto Done;
        }
    }

    hRes = CLSIDFromString (WSL_NETWORK_GUID, &NetworkGUID);

    if (S_OK != hRes)
    {
        printf ("Cannot convert NetworkGUID\n");
        goto Done;
    }

    hRes = HcnOpenNetwork (NetworkGUID, &pHcnNetwork, &pErrRecord);

    if (S_OK == hRes)
    {
        CoTaskMemFree (pErrRecord);
        pErrRecord = NULL;

        if (pHcnNetwork)
        {
            hRes = HcnCloseNetwork (pHcnNetwork);
            pHcnNetwork = NULL;
        }

        hRes = HcnDeleteNetwork (NetworkGUID, &pErrRecord);

        if (S_OK == hRes)
        {
            printf ("Existing WSL Network deleted\n");
        }
        else
        {
            printf ("Existing WSL Network cannot be deleted [%ls]!\n", pErrRecord);
            goto Done;
        }

        CoTaskMemFree (pErrRecord);
        pErrRecord = NULL;
    }

    hRes = HcnCreateNetwork (NetworkGUID, szwJsonBuffer, &pHcnNetwork, &pErrRecord);

    if (S_OK == hRes)
    {
        printf ("WSL Network created\n");
        ret = 0;
    }
    else
    {
        printf ("WSL Network not created [%ls]!\n", pErrRecord);
    }

    CoTaskMemFree (pErrRecord);
    pErrRecord = NULL;

    // Set the address we want in the sub-net created as an alias

    snprintf (szBuffer, sizeof (szBuffer), "wsl sudo ip a add %s/24 dev eth0", szIpAddress);
    system (szBuffer);

    printf ("\n");
    printf ("WSL Network setup done\n");
    printf ("----------------------------------------\n");
    printf ("Address: [%s]\n",    szIpAddress);
    printf ("Network: [%ls]\n",   szwIpNetwork);
    printf ("Gateway: [%ls]\n\n", szwIpGateway);

    if (dwWslHostPID)
    {
        printf ("Note: WSL is already running - Use 'wsl --shutdown' and restart to enforce new settings!\n\n");
    }

Done:

    if (pHcnEndpoint)
    {
        hRes = HcnCloseEndpoint (pHcnEndpoint);
        pHcnEndpoint = NULL;
    }

    if (pHcnNetwork)
    {
        hRes = HcnCloseNetwork (pHcnNetwork);
        pHcnNetwork = NULL;
    }

    return ret;
}

int main (int argc, char const *argv[])
{
    int ret = 0;

    if (argc > 1)
        ret = WSL_CreateUpdateNetwork (argv[1]);
    else
        ret = WSL_CreateUpdateNetwork ("");

    return ret;
}
