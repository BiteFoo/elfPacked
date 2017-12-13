package com.loopher.loader;

import android.content.Context;
import android.content.res.AssetManager;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.Objects;

/**
 * Created by John.Lu on 2017/12/13.
 * 这里将so文件复制到file目录
 */

public class AssetsUtils {
    public static boolean copyFile(Context context,String filenane)
    {
        if(context == null || isEmpty(filenane))
        {
            Log.e("Loopher","AssetsUtils  copyFile was Error");
            return  false;
        }
        AssetManager assetManager = context.getAssets();
        try {
            InputStream inputStream = assetManager.open(filenane);
            String path = context.getFilesDir().getAbsolutePath();
            String file = path+ File.separator+filenane;
            Log.d("Loopher","file path ="+file);
            FileOutputStream fileOutputStream = new FileOutputStream(new File(file));
            int len =-1;
            byte[] buff =new byte[1024];
            while (true)
            {
                len= inputStream.read(buff);
                if(len ==-1){
                    break;
                }
                fileOutputStream.write(buff,0,len);
            }
            inputStream.close();
            fileOutputStream.close();
            Log.d("Loopher","copy assets file ok ");
            return true;
        } catch (IOException e) {
            e.printStackTrace();
            Log.d("Loopher","copy assets file  exception  ");
            return false;
        }

    }
    private static  boolean isEmpty(String str){
        return str.equals("");
    }

}
