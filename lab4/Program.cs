using System;
using System.Collections.Generic;
using System.Threading;

public static class Program
{
    public static void Main(string[] args)
    {
        string[] urls =
        [
            "http://httpbin.org/bytes/1024",
            "http://httpbin.org/bytes/2048",
            "http://httpbin.org/bytes/4096",
            "http://httpbin.org/bytes/8192",
            "http://httpbin.org/bytes/16384",
            "http://httpbin.org/bytes/32768",
            "http://httpbin.org/bytes/65536"
        ];

        // RunCallbacksMode(urls);
        RunTasksMode(urls);
        // RunAwaitMode(urls);
    }

    private static void RunCallbacksMode(string[] urls)
    {
        using (var countdown = new CountdownEvent(urls.Length))
        {
            for (int i = 0; i < urls.Length; i++)
            {
                string url = urls[i];
                string outputFile = $"callbacks_file{i + 1}.bin";
                var downloader = new CallbackHttpDownloader(url, outputFile, countdown);
                downloader.Start();
            }
            countdown.Wait();
        }
        Console.WriteLine("Done");
    }

    
    private static void RunTasksMode(string[] urls)
    {
        var tasks = new List<Task>();
        for (int i = 0; i < urls.Length; i++)
        {
            string url = urls[i];
            string outputFile = $"tasks_file{i + 1}.bin";
            tasks.Add(TaskHttpDownloader.DownloadWithTasks(url, outputFile));
        }
        Task.WaitAll(tasks.ToArray());
        Console.WriteLine("Done");
    }

    private static void RunAwaitMode(string[] urls)
    {
        var tasks = new List<Task>();
        for (int i = 0; i < urls.Length; i++)
        {
            string url = urls[i];
            string outputFile = $"await_file{i + 1}.bin";
            tasks.Add(AwaitHttpDownloader.DownloadFileAsync(url, outputFile));
        }
        Task.WaitAll(tasks.ToArray());
        Console.WriteLine("Done");
    }
}