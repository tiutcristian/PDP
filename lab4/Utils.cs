using System;
using System.Net;
using System.Net.Sockets;
using System.Threading.Tasks;

public class Utils
{
    public static string BuildRequest(Uri uri)
    {
        string host = uri.Host;
        string path = string.IsNullOrEmpty(uri.PathAndQuery) ? "/" : uri.PathAndQuery;

        return
            $"GET {path} HTTP/1.1\r\n" +
            $"Host: {host}\r\n" +
            "Connection: close\r\n" +
            "\r\n";
    }

    public static IPEndPoint CreateEndPoint(Uri uri)
    {
        string host = uri.Host;
        int port = uri.Port == -1 ? 80 : uri.Port;

        var hostEntry = Dns.GetHostEntry(host);
        return new IPEndPoint(hostEntry.AddressList[0], port);
    }

    public static int FindHeaderEnd(byte[] buffer, int length)
    {
        for (int i = 0; i < length - 3; i++)
        {
            if (buffer[i] == (byte)'\r' && buffer[i + 1] == (byte)'\n' &&
                buffer[i + 2] == (byte)'\r' && buffer[i + 3] == (byte)'\n')
            {
                return i;
            }
        }
        return -1;
    }

    public static int ParseContentLength(string header)
    {
        var lines = header.Split(
            new[] { "\r\n" },
            StringSplitOptions.RemoveEmptyEntries);

        foreach (var line in lines)
        {
            if (line.StartsWith("Content-Length:", StringComparison.OrdinalIgnoreCase))
            {
                string value = line.Substring("Content-Length:".Length).Trim();
                if (int.TryParse(value, out int len))
                    return len;
            }
        }
        return -1;
    }
}