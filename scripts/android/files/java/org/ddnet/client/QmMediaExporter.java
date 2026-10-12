package org.ddnet.client;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ClipData;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.provider.DocumentsContract;
import android.util.Log;
import android.widget.Toast;

import androidx.core.content.FileProvider;
import com.qmclient.client.R;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;
import java.util.UUID;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public final class QmMediaExporter {
	private static final int REQUEST_EXPORT = 0x514d;
	private final Activity activity;
	private final ExecutorService worker = Executors.newSingleThreadExecutor();
	private final Set<String> excludedPaths = new HashSet<>();
	private File root;
	private boolean videos;
	private String pendingPath;
	private boolean busy;
	private AlertDialog dialog;
	private volatile boolean closed;

	public QmMediaExporter(Activity activity, Bundle savedState) {
		this.activity = activity;
		if(savedState != null) {
			pendingPath = savedState.getString("qm.media.export.path");
			busy = pendingPath != null;
		}
	}

	public void saveState(Bundle state) {
		if(pendingPath != null)
			state.putString("qm.media.export.path", pendingPath);
	}

	public void close() {
		closed = true;
		worker.shutdownNow();
		if(dialog != null)
			dialog.dismiss();
	}

	private void notifyUser(int message) {
		if(!closed && !activity.isFinishing() && !activity.isDestroyed())
			Toast.makeText(activity, message, Toast.LENGTH_LONG).show();
	}

	public void browse(String directory, boolean videos, String[] excluded) {
		if(busy || dialog != null || closed)
			return;
		try {
			root = new File(directory).getCanonicalFile();
			this.videos = videos;
			excludedPaths.clear();
			for(String path : excluded)
				excludedPaths.add(new File(path).getCanonicalPath());
			showDirectory(root);
		} catch(IOException | SecurityException error) {
			notifyUser(R.string.qm_media_failed);
		}
	}

	private boolean allowed(File file) throws IOException {
		return QmMediaFiles.contains(root, file) && !excludedPaths.contains(file.getCanonicalPath());
	}

	private void showDirectory(File directory) throws IOException {
		if(!allowed(directory))
			throw new IOException("Outside media directory");
		File[] files = directory.listFiles();
		if(files == null) {
			notifyUser(directory.exists() ? R.string.qm_media_failed : R.string.qm_media_empty);
			return;
		}
		Arrays.sort(files, (left, right) -> {
			if(left.isDirectory() != right.isDirectory())
				return left.isDirectory() ? -1 : 1;
			return left.getName().compareToIgnoreCase(right.getName());
		});
		ArrayList<File> entries = new ArrayList<>();
		ArrayList<String> names = new ArrayList<>();
		if(!directory.equals(root)) {
			entries.add(directory.getParentFile());
			names.add("../");
		}
		for(File file : files) {
			if(allowed(file) && (file.isDirectory() || (file.isFile() && QmMediaFiles.matches(file, videos)))) {
				entries.add(file);
				names.add(file.getName() + (file.isDirectory() ? "/" : ""));
			}
		}
		if(entries.isEmpty()) {
			notifyUser(R.string.qm_media_empty);
			return;
		}
		dialog = new AlertDialog.Builder(activity)
			.setTitle(videos ? R.string.qm_media_videos : R.string.qm_media_demos)
			.setItems(names.toArray(new String[0]), (sender, index) -> {
				dialog = null;
				try {
					File selected = entries.get(index);
					if(!allowed(selected))
						throw new IOException("Invalid selection");
					if(selected.isDirectory())
						showDirectory(selected);
					else
						showActions(selected);
				} catch(IOException | SecurityException error) {
					notifyUser(R.string.qm_media_failed);
				}
			})
			.setNegativeButton(android.R.string.cancel, null)
			.create();
		AlertDialog current = dialog;
		current.setOnDismissListener(sender -> { if(dialog == current) dialog = null; });
		current.show();
	}

	private void showActions(File file) {
		CharSequence[] actions = {activity.getText(R.string.qm_media_export), activity.getText(R.string.qm_media_share)};
		dialog = new AlertDialog.Builder(activity)
			.setTitle(file.getName())
			.setItems(actions, (sender, index) -> {
				dialog = null;
				if(index == 0)
					export(file);
				else
					share(file);
			})
			.setNegativeButton(android.R.string.cancel, null)
			.create();
		AlertDialog current = dialog;
		current.setOnDismissListener(sender -> { if(dialog == current) dialog = null; });
		current.show();
	}

	private String mimeType(File file) {
		return QmMediaFiles.matches(file, true) ? "video/mp4" : "application/octet-stream";
	}

	private void export(File file) {
		pendingPath = file.getAbsolutePath();
		busy = true;
		Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT)
			.addCategory(Intent.CATEGORY_OPENABLE)
			.setType(mimeType(file))
			.putExtra(Intent.EXTRA_TITLE, file.getName());
		try {
			activity.startActivityForResult(intent, REQUEST_EXPORT);
		} catch(RuntimeException error) {
			pendingPath = null;
			busy = false;
			notifyUser(R.string.qm_media_failed);
		}
	}

	public boolean onActivityResult(int requestCode, int resultCode, Intent data) {
		if(requestCode != REQUEST_EXPORT)
			return false;
		if(closed)
			return true;
		String source = pendingPath;
		pendingPath = null;
		if(resultCode != Activity.RESULT_OK || source == null || data == null || data.getData() == null) {
			busy = false;
			return true;
		}
		Uri target = data.getData();
		notifyUser(R.string.qm_media_copying);
		worker.execute(() -> {
			boolean success = false;
			try {
				try(FileInputStream input = new FileInputStream(source);
					OutputStream output = activity.getContentResolver().openOutputStream(target, "w")) {
					if(output == null)
						throw new IOException("No output stream");
					QmMediaFiles.copy(input, output);
				}
				success = true;
			} catch(IOException | RuntimeException error) {
				Log.w("QmMediaExporter", "Export failed", error);
			}
			if(!success) {
				// 仅清理由本次 CREATE_DOCUMENT 新建的未完成文件。
				try { DocumentsContract.deleteDocument(activity.getContentResolver(), target); }
				catch(Exception ignored) { }
			}
			final boolean completed = success;
			activity.runOnUiThread(() -> {
				busy = false;
				notifyUser(completed ? R.string.qm_media_exported : R.string.qm_media_failed);
			});
		});
		return true;
	}

	private void share(File source) {
		busy = true;
		notifyUser(R.string.qm_media_copying);
		worker.execute(() -> {
			File staged = null;
			try {
				File directory = new File(activity.getCacheDir(), "qm-media/" + UUID.randomUUID());
				if(!directory.mkdirs())
					throw new IOException("Could not prepare share directory");
				staged = new File(directory, source.getName());
				try(FileInputStream input = new FileInputStream(source); FileOutputStream output = new FileOutputStream(staged)) {
					QmMediaFiles.copy(input, output);
				}
				final Uri uri = FileProvider.getUriForFile(activity, activity.getPackageName() + ".qm.media", staged);
				activity.runOnUiThread(() -> {
					busy = false;
					if(closed)
						return;
					Intent intent = new Intent(Intent.ACTION_SEND).setType(mimeType(source))
						.putExtra(Intent.EXTRA_STREAM, uri)
						.setClipData(ClipData.newRawUri("media", uri))
						.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
					try { activity.startActivity(Intent.createChooser(intent, activity.getText(R.string.qm_media_share))); }
					catch(RuntimeException error) { notifyUser(R.string.qm_media_failed); }
				});
			} catch(IOException | RuntimeException error) {
				if(staged != null)
					staged.delete();
				Log.w("QmMediaExporter", "Share failed", error);
				activity.runOnUiThread(() -> { busy = false; notifyUser(R.string.qm_media_failed); });
			}
		});
	}
}
