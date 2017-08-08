/***************************************************************************
 * Copyright (C) 2017 Kaloyan Raev
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser
            android:label="@string/title_browse" General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ***************************************************************************/
package name.raev.kaloyan.hellostorj.jni;

import android.support.annotation.NonNull;

import java.io.Serializable;

public class File implements Serializable, Comparable<File> {

    private String id;
    private String name;
    private String created;
    private boolean decrypted;
    private long size;
    private String mimeType;
    private String erasure;
    private String index;
    private String hmac;

    public File(String id,
                String name,
                String created,
                boolean decrypted,
                long size,
                String mimeType,
                String erasure,
                String index,
                String hmac) {
        this.id = id;
        this.name = name;
        this.created = created;
        this.decrypted = decrypted;
        this.size = size;
        this.mimeType = mimeType;
        this.erasure = erasure;
        this.index = index;
        this.hmac = hmac;
    }

    public String getId() {
        return id;
    }

    public String getName() {
        return name;
    }

    public String getCreated() {
        return created;
    }

    public boolean isDecrypted() {
        return decrypted;
    }

    public long getSize() {
        return size;
    }

    public String getMimeType() {
        return mimeType;
    }

    public String getErasure() {
        return erasure;
    }

    public String getIndex() {
        return index;
    }

    public String getHMAC() {
        return hmac;
    }

    @Override
    public int compareTo(@NonNull File other) {
        return name.compareTo(other.name);
    }
}
