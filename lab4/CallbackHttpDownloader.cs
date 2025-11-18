public class CallbackHttpDownloader
{
    private readonly Uri _uri;
    private readonly string _outputFile;
    private readonly CountdownEvent _countdown;

    private readonly Socket _socket;
    private readonly byte[] _buffer = new byte[8192];

    private readonly List<byte> _headerBuffer = new List<byte>();
    private bool _headersParsed = false;
    private int _contentLength = -1;
    private int _bodyBytesRead = 0;
    private FileStream _fileStream;

    public CallbackHttpDownloader(string url, string outputFile, CountdownEvent countdown)
    {
        _uri = new Uri(url);
        _outputFile = outputFile;
        _countdown = countdown;
        _socket = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
    }

    public void Start()
    {
        Console.WriteLine($"[callbacks] Start: {_uri} -> {_outputFile}");
        var ep = Program.CreateEndPoint(_uri);

        _socket.BeginConnect(ep, OnConnected, null);
    }

    private void OnConnected(IAsyncResult ar)
    {
        try
        {
            _socket.EndConnect(ar);

            string request = Program.BuildRequest(_uri);
            byte[] requestBytes = Encoding.ASCII.GetBytes(request);

            _socket.BeginSend(requestBytes, 0, requestBytes.Length, SocketFlags.None, OnSent, null);
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[callbacks] Error connecting {_uri}: {ex.Message}");
            Finish();
        }
    }

    private void OnSent(IAsyncResult ar)
    {
        try
        {
            int sent = _socket.EndSend(ar);
            if (sent <= 0)
            {
                Console.WriteLine($"[callbacks] Failed to send request for {_uri}");
                Finish();
                return;
            }

            _socket.BeginReceive(_buffer, 0, _buffer.Length, SocketFlags.None, OnReceived, null);
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[callbacks] Error sending {_uri}: {ex.Message}");
            Finish();
        }
    }

    private void OnReceived(IAsyncResult ar)
    {
        int n;
        try
        {
            n = _socket.EndReceive(ar);
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[callbacks] Error receiving {_uri}: {ex.Message}");
            Finish();
            return;
        }

        if (n <= 0)
        {
            Finish();
            return;
        }

        if (!_headersParsed)
        {
            for (int i = 0; i < n; i++)
                _headerBuffer.Add(_buffer[i]);

            byte[] all = _headerBuffer.ToArray();
            int headerEnd = Program.FindHeaderEnd(all, all.Length);

            if (headerEnd >= 0)
            {
                string headerString = Encoding.ASCII.GetString(all, 0, headerEnd);
                _contentLength = Program.ParseContentLength(headerString);

                Console.WriteLine("[callbacks] Headers:");
                Console.WriteLine(headerString);
                Console.WriteLine($"[callbacks] Content-Length: {_contentLength}");

                _fileStream = new FileStream(_outputFile, FileMode.Create, FileAccess.Write);

                int bodyStart = headerEnd + 4;
                int bodyLenInAccumulator = all.Length - bodyStart;
                if (bodyLenInAccumulator > 0)
                {
                    _fileStream.Write(all, bodyStart, bodyLenInAccumulator);
                    _bodyBytesRead += bodyLenInAccumulator;
                }

                _headersParsed = true;
                _headerBuffer.Clear();
            }
        }
        else
        {
            _fileStream.Write(_buffer, 0, n);
            _bodyBytesRead += n;
        }

        if (_headersParsed && _contentLength > 0 && _bodyBytesRead >= _contentLength)
        {
            Finish();
            return;
        }

        _socket.BeginReceive(_buffer, 0, _buffer.Length, SocketFlags.None, OnReceived, null);
    }

    private void Finish()
    {
        try { _fileStream?.Close(); } catch { }
        try
        {
            _socket?.Shutdown(SocketShutdown.Both);
        }
        catch { }
        try { _socket?.Close(); } catch { }

        Console.WriteLine($"[callbacks] Finished: {_uri} -> {_outputFile}");
        _countdown.Signal();
    }
}