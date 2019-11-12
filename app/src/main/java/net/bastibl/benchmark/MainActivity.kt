package net.bastibl.benchmark

import android.Manifest
import android.annotation.SuppressLint
import android.content.pm.PackageManager
import android.os.Bundle
import android.os.Environment
import android.util.Log
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import kotlinx.android.synthetic.main.activity_main.*
import java.io.File
import kotlin.concurrent.thread

class MainActivity : AppCompatActivity() {

    private var haveStoragePermission : Boolean = false

    private fun checkStoragePermission() {
        if (ContextCompat.checkSelfPermission(this,
                Manifest.permission.WRITE_EXTERNAL_STORAGE)
            != PackageManager.PERMISSION_GRANTED) {

            ActivityCompat.requestPermissions(this,
                arrayOf(Manifest.permission.WRITE_EXTERNAL_STORAGE),
                123)
        } else {
            haveStoragePermission = true
        }
    }

    override fun onRequestPermissionsResult(requestCode: Int,
                                            permissions: Array<String>, grantResults: IntArray) {
        when (requestCode) {
            123 -> {
                if ((grantResults.isNotEmpty() && grantResults[0] == PackageManager.PERMISSION_GRANTED)) {
                    haveStoragePermission = true
                }
            }
        }
    }

    private var thread : Thread? = null;

    @SuppressLint("SetTextI18n")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        checkStoragePermission()

        btn_msg.setOnClickListener {
            if(!haveStoragePermission) {
                sample_text.text = "no storage permssion"
                return@setOnClickListener
            }

            thread?.let {
                if(thread?.isAlive == true) {
                    sample_text.text = "measurement is running"
                    return@setOnClickListener
                }
            }

            sample_text.text = "starting benchmark"
            var f = File(Environment.getExternalStorageDirectory(),"/volk/");
            f.mkdirs();
            f = File(Environment.getExternalStorageDirectory(),"/gnuradio/");
            f.mkdirs();

            val tmpDir = cacheDir.absolutePath

            thread = thread(start = true) {

                f = File(Environment.getExternalStorageDirectory(),"/gnuradio/benchmark_msg.csv");

                f.printWriter().use {out ->

                    out.println("run,pipes,stages,repetition,burst_size,pdu_size,time")

                    for(run in 0..9) {
                        for(burst_size in intArrayOf(1000)) {
                            for (stage in 1..10) {
                                Log.d("benchmark", String.format("running: run %d, stages %d, burst size %d", run, stage, burst_size))
                                val result =
                                    runMsg(tmpDir, stage, stage, burst_size,1000, 10, run)
                                out.print(result);
                            }
                        }
                    }
                }
                runOnUiThread {
                    sample_text.text = "done"
                }
            }
        }

        btn_stream.setOnClickListener {
            if(!haveStoragePermission) {
                sample_text.text = "no storage permssion"
                return@setOnClickListener
            }

            thread?.let {
                if(thread?.isAlive == true) {
                    sample_text.text = "measurement is running"
                    return@setOnClickListener
                }
            }

            sample_text.text = "starting benchmark"
            var f = File(Environment.getExternalStorageDirectory(),"/volk/");
            f.mkdirs();
            f = File(Environment.getExternalStorageDirectory(),"/gnuradio/");
            f.mkdirs();

            val tmpDir = cacheDir.absolutePath

            thread = thread(start = true) {

                f = File(Environment.getExternalStorageDirectory(),"/gnuradio/benchmark_buf.csv");

                f.printWriter().use {out ->

                    out.println("run,pipes,stages,samples,time")

                    for(run in 0..9) {
                        for (stage in 1..10) {
                            Log.d("benchmark", String.format("running: run %d, stages %d", run, stage))
                            val result =
                                runBuf(tmpDir, stage, stage, 200000000, run)
                            out.print(result);
                        }
                    }
                }
                runOnUiThread {
                    sample_text.text = "done"
                }
            }
        }

        btn_cl.setOnClickListener {
            if(!haveStoragePermission) {
                sample_text.text = "no storage permssion"
                return@setOnClickListener
            }

            thread?.let {
                if(thread?.isAlive == true) {
                    sample_text.text = "measurement is running"
                    return@setOnClickListener
                }
            }

            sample_text.text = "starting cl benchmark"
            var f = File(Environment.getExternalStorageDirectory(),"/volk/");
            f.mkdirs();
            f = File(Environment.getExternalStorageDirectory(),"/gnuradio/");
            f.mkdirs();

            val tmpDir = cacheDir.absolutePath

            thread = thread(start = true) {

                f = File(Environment.getExternalStorageDirectory(),"/gnuradio/benchmark_cl.csv");

                f.printWriter().use {out ->

                    out.println("kernel,dev,blocksize,rep,elapsed")

                    var tmp = runCl(tmpDir)
                    out.print(tmp)
                }

                runOnUiThread {
                    sample_text.text = "done"
                }
            }
        }
    }

    private external fun runMsg(tmpDir : String, pipes: Int, stages : Int, bust_size : Int,
                                pdu_size : Int, repetitions : Int, run : Int): String
    private external fun runBuf(tmpDir : String, pipes: Int, stages : Int, samples : Int,
                                run : Int): String
    private external fun runCl(tmpDir : String): String

    companion object {
        init {
            System.loadLibrary("native-lib")
        }
    }
}
