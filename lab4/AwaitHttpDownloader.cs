using System;
using System.Collections.Generic;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading.Tasks;

public static class AwaitHttpDownloader
{
    private static Task ConnectAsync(Socket socket, EndPoint ep)
    {
        var tcs = new TaskCompletionSource<bool>();
        socket.BeginConnect(ep, ar =>
        {
            try
            {
                socket.EndConnect(ar);
                tcs.SetResult(true);
            }
            catch (Exception ex)
            {
                tcs.SetException(ex);
            }
        }, null);
        return tcs.Task;
    }

    private static Task<int> SendAsync(Socket socket, byte[] buffer, int offset, int size)
    {
        var tcs = new TaskCompletionSource<int>();
        socket.BeginSend(buffer, offset, size, SocketFlags.None, ar =>
        {
            try
            {
                int sent = socket.EndSend(ar);
                tcs.SetResult(sent);
            }
            catch (Exception ex)
            {
                tcs.SetException(ex);
            }
        }, null);
        return tcs.Task;
    }

    private static Task<int> ReceiveAsync(Socket socket, byte[] buffer, int offset, int size)
    {
        var tcs = new TaskCompletionSource<int>();
        socket.BeginReceive(buffer, offset, size, SocketFlags.None, ar =>
        {
            try
            {
                int received = socket.EndReceive(ar);
                tcs.SetResult(received);
            }
            catch (Exception ex)
            {
                tcs.SetException(ex);
            }
        }, null);
        return tcs.Task;
    }

    public static async Task DownloadFileAsync(string url, string outputPath)
    {
        Uri uri = new Uri(url);
        Console.WriteLine($"[await] Start: {uri} -> {outputPath}");

        var ep = Utils.CreateEndPoint(uri);
        using (var socket = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp))
        {
            await ConnectAsync(socket, ep);

            string request = Utils.BuildRequest(uri);
            byte[] requestBytes = Encoding.ASCII.GetBytes(request);

            int totalSent = 0;
            while (totalSent < requestBytes.Length)
            {
                int sent = await SendAsync(socket, requestBytes, totalSent, requestBytes.Length - totalSent);
                if (sent <= 0)
                    throw new IOException("Failed to send HTTP request.");
                totalSent += sent;
            }

            await ReceiveAndSaveAsync(socket, outputPath);
        }

        Console.WriteLine($"[await] Finished: {url} -> {outputPath}");
    }

    private static async Task ReceiveAndSaveAsync(Socket socket, string outputPath)
    {
        byte[] buffer = new byte[8192];
        var accumulator = new List<byte>();

        bool headersParsed = false;
        int contentLength = -1;
        int bodyBytesRead = 0;
        FileStream fileStream = null;

        try
        {
            while (true)
            {
                int n = await ReceiveAsync(socket, buffer, 0, buffer.Length);
                if (n <= 0)
                {
                    break;
                }

                if (!headersParsed)
                {
                    for (int i = 0; i < n; i++)
                        accumulator.Add(buffer[i]);

                    byte[] all = accumulator.ToArray();
                    int headerEnd = Utils.FindHeaderEnd(all, all.Length);

                    if (headerEnd >= 0)
                    {
                        string headerString = Encoding.ASCII.GetString(all, 0, headerEnd);
                        contentLength = Utils.ParseContentLength(headerString);

                        Console.WriteLine("[await] Headers:");
                        Console.WriteLine(headerString);
                        Console.WriteLine($"[await] Content-Length: {contentLength}");

                        fileStream = new FileStream(outputPath, FileMode.Create, FileAccess.Write);

                        int bodyStart = headerEnd + 4;
                        int bodyLenInAccumulator = all.Length - bodyStart;
                        if (bodyLenInAccumulator > 0)
                        {
                            await fileStream.WriteAsync(all, bodyStart, bodyLenInAccumulator);
                            bodyBytesRead += bodyLenInAccumulator;
                        }

                        headersParsed = true;
                        accumulator.Clear();
                    }
                }
                else
                {
                    await fileStream.WriteAsync(buffer, 0, n);
                    bodyBytesRead += n;
                }

                if (headersParsed && contentLength > 0 && bodyBytesRead >= contentLength)
                {
                    break;
                }
            }
        }
        finally
        {
            if (fileStream != null)
            {
                await fileStream.FlushAsync();
                fileStream.Close();
            }
        }
    }
}