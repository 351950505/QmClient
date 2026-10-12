package org.ddnet.client;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.io.InterruptedIOException;
import java.io.OutputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;

// 无 Android 设备依赖；传入工作树 tmp 下的隔离测试目录。
public final class QmMediaFilesTest {
	private static void check(boolean condition, String message) {
		if(!condition)
			throw new AssertionError(message);
	}

	private static void directoryBoundary(Path temp) throws Exception {
		File root = temp.resolve("recordings").toFile();
		Files.createDirectories(root.toPath());
		check(QmMediaFiles.contains(root, new File(root, "auto/录像.demo")), "Nested media must be accessible");
		check(!QmMediaFiles.contains(root, new File(root, "../settings.cfg")), "Parent traversal must be rejected");
		check(!QmMediaFiles.contains(root, temp.resolve("recordings-other/run.demo").toFile()), "Sibling prefix must not match");
	}

	private static void mediaTypes() {
		check(QmMediaFiles.matches(new File("run.DEMO"), false), "Uppercase demo extension");
		check(QmMediaFiles.matches(new File("录像.MP4"), true), "Unicode video filename");
		check(!QmMediaFiles.matches(new File("settings.cfg"), false), "Config must not be listed");
		check(!QmMediaFiles.matches(new File("run.demo.tmp"), false), "Temporary recording must not be listed");
		check(!QmMediaFiles.matches(new File("run.demo"), true), "Video list must exclude demos");
	}

	private static void largeCopyKeepsBytes() throws Exception {
		byte[] source = new byte[200000];
		for(int i = 0; i < source.length; ++i)
			source[i] = (byte)i;
		ByteArrayOutputStream output = new ByteArrayOutputStream();
		QmMediaFiles.copy(new ByteArrayInputStream(source), output);
		check(Arrays.equals(source, output.toByteArray()), "All copy chunks must be retained");
	}

	private static void writeFailurePropagates() throws Exception {
		OutputStream failing = new OutputStream() {
			@Override public void write(int value) throws IOException { throw new IOException("Disk full"); }
		};
		try {
			QmMediaFiles.copy(new ByteArrayInputStream(new byte[] {1}), failing);
			throw new AssertionError("Copy must report output failure");
		} catch(IOException expected) { }
	}

	private static void cancellationPropagates() throws Exception {
		Thread.currentThread().interrupt();
		try {
			QmMediaFiles.copy(new ByteArrayInputStream(new byte[] {1}), new ByteArrayOutputStream());
			throw new AssertionError("Cancelled copy must not finish successfully");
		} catch(InterruptedIOException expected) {
		} finally {
			Thread.interrupted();
		}
	}

	public static void main(String[] arguments) throws Exception {
		if(arguments.length != 1)
			throw new IllegalArgumentException("Pass a workspace tmp directory");
		directoryBoundary(Path.of(arguments[0]));
		mediaTypes();
		largeCopyKeepsBytes();
		writeFailurePropagates();
		cancellationPropagates();
		System.out.println("QmMediaFiles: 5 scenarios passed");
	}
}
